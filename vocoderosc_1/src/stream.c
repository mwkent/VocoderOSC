/** stream.c
 *  Everything necessary for a PortAudio stream on a computer running the both
 *  the vocoder and synthesizer.
 *  Takes input from a microphone, generates a new synthesizer sample, and ouputs
 *  the result of applying the vocoder process to them.
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "portaudio.h"
#include "../../headers/synth.h"
#include "../../headers/vocoder.h"

#define FRAMES_PER_BUFFER   (256)
#define NUM_CHANNELS        (1)
#define SAMPLE_RATE         (44100)
#define FORMAT              paFloat32

typedef struct {
    Synth*   synth;
    Vocoder* vc;
} paData;

/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int paCallback( const void    *inputBuffer,
                       void          *outputBuffer,
                       unsigned long framesPerBuffer,
                       const         PaStreamCallbackTimeInfo* timeInfo,
                       PaStreamCallbackFlags statusFlags,
                       void          *userData )
{
    /* To stop unused variable warnings */    
    (void) timeInfo;
    (void) statusFlags;
    unsigned int i;

    /* Cast data passed through the stream */
    const float *in = (const float*)inputBuffer;
    float *out      = (float*)outputBuffer;
    paData* data    = (paData*)userData;

    float synthSample;
    if( inputBuffer == NULL) {
        for (i = 0; i < framesPerBuffer; i++) {
            *out++ = 0;
        }
    } else {
        for(i = 0; i < framesPerBuffer; i++) {
            synthSample = synth_getNext(data->synth);
            // synthSample = 2*(float)rand()/(float)RAND_MAX - 1; // white noise test
            *out++ = vc_process(*in++, synthSample, i, framesPerBuffer, data->vc);
            // *out++ = (*in++) + synthSample; // Output the synth added and input
            // *out++ = synthSample; // Just output the synthesizer
            // *out++ = (*in++); // Output the voice input
        }
    }
    return 0;
}

/*******************************************************************/
PaStream *stream;
PaError err;

int openPA(Synth* synth, Vocoder* vc);
int closePA();

int openPA(Synth* synth, Vocoder* vc) {
    printf("PortAudio Test: output sawtooth wave.\n"); fflush(stdout);

    PaStreamParameters inputParameters, outputParameters;

    paData* data = (paData*) malloc(sizeof(paData));
    data->synth  = synth;
    data->vc     = vc;

    /* Initialize library before making any other calls. */
    err = Pa_Initialize();
    if( err != paNoError ) goto error;

    /* Set up input parameters */
    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default input device.\n");
        goto error;
    }
    printf( "Input device # %d.\n", inputParameters.device );
    printf( "Input LL: %g s\n", Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency );
    printf( "Input HL: %g s\n", Pa_GetDeviceInfo( inputParameters.device )->defaultHighInputLatency );
    inputParameters.channelCount = NUM_CHANNELS;
    inputParameters.sampleFormat = FORMAT;
    inputParameters.suggestedLatency = 
        Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Set up output parameters */
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default output device.\n");
        goto error;
    }
    printf( "Output device # %d.\n", outputParameters.device );
    printf( "Output LL: %g s\n", Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency );
    printf( "Output HL: %g s\n", Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency );
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat = FORMAT;
    outputParameters.suggestedLatency = 
        Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    
    /* Open an audio I/O stream. */
    err = Pa_OpenStream( 
            &stream,
            &inputParameters,
            &outputParameters,
            SAMPLE_RATE,
            FRAMES_PER_BUFFER,
            paClipOff,
            paCallback,
            data );
    if( err != paNoError ) goto error;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;

    return err;

error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    synth_free(data->synth);
    vc_free(data->vc);
    free(data);
    return err;
}

int closePA() {
    /* Stop playback */
    err = Pa_StopStream( stream );
    if( err != paNoError ) goto error;
    
    /* Close the stream */
    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto error;
    
    /* Terminate PortAudio */
    err = Pa_Terminate();
    if( err != paNoError ) goto error;
    printf("PortAudio terminated.\n");
    return err;

error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return err;
}
