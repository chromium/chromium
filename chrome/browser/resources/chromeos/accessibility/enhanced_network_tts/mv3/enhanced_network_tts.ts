// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

type AudioBuffer = chrome.ttsEngine.AudioBuffer;
type AudioStreamOptions = chrome.ttsEngine.AudioStreamOptions;
type SpeakOptions = chrome.ttsEngine.SpeakOptions;

interface ServerResponse {
  response: ash.enhancedNetworkTts.mojom.TtsResponse;
  sampleRate: number;
  bufferSize: number;
  sendTtsAudio: (foo: chrome.ttsEngine.AudioBuffer) => void;
  sendError: (foo: string) => void;
}

/**
 * This class contains the main functionalities for Enhanced Network TTS.
 */
export class EnhancedNetworkTts {
  /**
   * The queue for processing server responses. Each element has one
   * |response| received from mojo, and two callbacks |sendTtsAudio| and
   * |sendError| for dispatching processed response data. We work on one
   * |response| at a time, decode the |response| at |sampleRate| and send the
   * decoded data to |sendTtsAudio| in buffers with |bufferSize|.
   */
  private processingQueue_: ServerResponse[];

  /** The mojo API for Enhanced Network TTS. */
  private api_: EnhancedNetworkTtsAdapter|null;

  constructor() {
    this.processingQueue_ = [];
    this.api_ = null;

    /**
     * Register the listener for onSpeakWithAudioStream event. The event will be
     * called when the user makes a call to tts.speak() and one of the voices
     * from this extension's manifest is the first to match the options object.
     */
    chrome.ttsEngine.onSpeakWithAudioStream.addListener(
        (utterance: string, options: SpeakOptions,
         audioStreamOptions: AudioStreamOptions,
         sendTtsAudio: (buffer: AudioBuffer) => void,
         sendError: (error: string) => void) =>
            this.onSpeakWithAudioStreamEvent(
                utterance, options, audioStreamOptions, sendTtsAudio,
                sendError));

    // The onStop listener is needed for the |tts_engine_events::kOnStop| check
    // in tts_engine_extension_api.cc
    chrome.ttsEngine.onStop.addListener(() => this.onStopEvent());
  }

  /**
   * Callback for onSpeakWithAudioStream event. This function responds to a TTS
   * request specified by the parameters |utterance|, |options|, and
   * |audioStreamOptions|. It uses the mojo API to safely retrieve audio that
   * fulfill the request and send the audio to |sendTtsAudio|.
   * @param utterance The text to speak.
   * @param options Options specified when a client calls the chrome.tts.speak.
   * @param audioStreamOptions Contains the audio stream format expected to be
   *     produced
   * by this engine.
   * @param sendTtsAudio A function that accepts AudioBuffer for playback.
   * @param sendError A function that signals error.
   */
  async onSpeakWithAudioStreamEvent(
      utterance: string, options: chrome.ttsEngine.SpeakOptions,
      audioStreamOptions: chrome.ttsEngine.AudioStreamOptions,
      sendTtsAudio: (audioBuffer: chrome.ttsEngine.AudioBuffer) => void,
      sendError: (errorMessage: string) => void): Promise<void> {
    this.processingQueue_ = [];

    if (!this.api_) {
      try {
        this.api_ =
            await chrome.mojoPrivate.requireAsync('ash.enhanced_network_tts') as
            EnhancedNetworkTtsAdapter;
      } catch (e) {
        console.warn(
            'Could not get mojoPrivate bindings: ' + (e as Error).message);
        sendError('Error: unable to get mojoPrivate bindings');
        return;
      }
    }

    const request = EnhancedNetworkTts.generateRequest(utterance, options);
    await this.api_.getAudioDataWithCallback(
        request,
        response => this.queueResponse_(
            response, audioStreamOptions, sendTtsAudio, sendError));
  }

  /**
   * Queue the |response| from the mojo API to be processed. If this is the
   * first response, we also start processing the response queue.
   * @param response
   * @param audioStreamOptions Contains the audio stream format expected to be
   *     produced
   * by this engine.
   * @param sendTtsAudio A function that accepts AudioBuffer for playback.
   * @param sendError A function that signals error.
   */
  private queueResponse_(
      response: ash.enhancedNetworkTts.mojom.TtsResponse,
      audioStreamOptions: chrome.ttsEngine.AudioStreamOptions,
      sendTtsAudio: (audioBuffer: chrome.ttsEngine.AudioBuffer) => void,
      sendError: (errorMessage: string) => void): void {
    const sampleRate = audioStreamOptions.sampleRate;
    const bufferSize = audioStreamOptions.bufferSize;
    this.processingQueue_.push(
        {response, sampleRate, bufferSize, sendTtsAudio, sendError});
    if (this.processingQueue_.length === 1) {
      this.processResponse_();
    }
  }

  /**
   * Process the top response from |this.processingQueue_|. Once finished, it
   * removes the processed response from the queue and calls itself to process
   * the next available one.
   */
  private async processResponse_(): Promise<void> {
    if (this.processingQueue_.length === 0) {
      return;
    }

    const {response, sampleRate, bufferSize, sendTtsAudio, sendError} =
        this.processingQueue_[0];

    if (!response.data) {
      console.warn('Could not get the data from Enhanced Network mojo API.');
      if (response.errorCode === undefined) {
        return;
      }

      switch (response.errorCode) {
        case ash.enhancedNetworkTts.mojom.TtsRequestError.kEmptyUtterance:
          sendError('Error: empty utterance');
          break;
        case ash.enhancedNetworkTts.mojom.TtsRequestError.kOverLength:
          sendError('Error: utterance too long');
          break;
        case ash.enhancedNetworkTts.mojom.TtsRequestError.kServerError:
          sendError('Error: unable to reach server');
          break;
        case ash.enhancedNetworkTts.mojom.TtsRequestError
            .kReceivedUnexpectedData:
          sendError('Error: unexpected data');
          break;
        case ash.enhancedNetworkTts.mojom.TtsRequestError.kRequestOverride:
          // not an error
          break;
      }
      return;
    }

    const lastData = response.data.lastData;
    const audioData = new Uint8Array(response.data.audio).buffer;
    const timeInfo = response.data.timeInfo;
    const decodedAudioData =
        await EnhancedNetworkTts.decodeAudioDataAtSampleRate(
            audioData, sampleRate);

    EnhancedNetworkTts.sendAudioDataInBuffers(
        decodedAudioData, sampleRate, bufferSize, timeInfo, sendTtsAudio,
        lastData);
    // Remove the current one.
    this.processingQueue_.shift();
    // Process the next available response.
    if (this.processingQueue_.length > 0) {
      await this.processResponse_();
    }
  }

  async onStopEvent(): Promise<void> {
    // Clear the prior responses from the server and reset states.
    this.processingQueue_ = [];
    if (this.api_) {
      this.api_.resetApi();
    }
  }

  /**
   * Generates a request that can be used to query audio data from the Enhanced
   * Network TTS mojo API, which sends the request to the ReadAloud server.
   * @param utterance The text to speak.
   * @param options Options specified when a client calls the chrome.tts.speak.
   */
  static generateRequest(
      utterance: string, options: chrome.ttsEngine.SpeakOptions):
      ash.enhancedNetworkTts.mojom.TtsRequest {
    // Gets the playback rate.
    const rate = options.rate || 1.0;

    // Unpack voice and lang. For lang, the server takes lang code only.
    let voice: string|undefined;
    let lang: string|undefined;
    const voiceStr = options.voiceName || '';
    let langStr = options.lang || '';
    langStr = langStr.trim().split(/_/)[0];

    // The ReadAloud server takes voice and lang as a pair. Sets them to
    // undefined if either is empty.
    if (voiceStr.trim().length === 0 || langStr.trim().length === 0) {
      voice = undefined;
      lang = undefined;
    } else if (voice === 'default-wavenet') {
      // The default voice is used for a situation where the user enables the
      // natural voices in Select-to-Speak but does not specify which voice name
      // to use. We override the |voice| and |lang| to |undefined| to get the
      // default voice from the server.
      voice = undefined;
      lang = undefined;
    } else {
      voice = voiceStr;
      lang = langStr;
    }

    return {utterance, rate, voice, lang};
  }

  /**
   * Decodes |inputAudioData| into a Float32Array at the |targetSampleRate|.
   * @param inputAudioData The original data from audio files like mp3, ogg, or
   *     wav.
   * @param targetSampleRate The sampling rate for decoding.
   */
  static async decodeAudioDataAtSampleRate(
      inputAudioData: ArrayBuffer,
      targetSampleRate: number): Promise<Float32Array|null> {
    const context = new AudioContext({sampleRate: targetSampleRate});

    let audioBuffer;
    try {
      audioBuffer = await context.decodeAudioData(inputAudioData);
    } catch (e) {
      console.warn('Could not decode audio data');
      return new Promise(resolve => resolve(null));
    } finally {
      // Release system resources.
      context.close();
    }

    if (!audioBuffer) {
      return new Promise(resolve => resolve(null));
    }
    return audioBuffer.getChannelData(0);
  }

  /**
   * Creates a subarray from |startIdx| in the input |array|, with the size of
   * |subarraySize|.
   * @param array
   * @param startIdx
   * @param subarraySize
   */
  static subarrayFrom(
      array: Float32Array, startIdx: number,
      subarraySize: number): Float32Array {
    const subarray = new Float32Array(subarraySize);
    subarray.set(
        array.subarray(
            startIdx, Math.min(startIdx + subarraySize, array.length)),
        0 /* offset */);
    return subarray;
  }

  /**
   * Sends |audioData| in AudioBuffers to |sendTtsAudio|. The AudioBuffer should
   * have length |bufferSize| at |sampleRate|, and is a linear PCM in the
   * Float32Array type. |sendTtsAudio|, |sampleRate|, and |bufferSize| are
   * provided by the |chrome.ttsEngine.onSpeakWithAudioStream| event.
   * |sampleRate| and |bufferSize| can be specified in the extension manifest
   * file.
   * @param audioData Decoded audio data with a |sampleRate|.
   * @param sampleRate
   * @param bufferSize The size of each buffer that |sendTtsAudio| expects.
   * @param timeInfo An array of timestamp information for each word in the
   * |audioData|.
   * @param sendTtsAudio The function that takes |AudioBuffer| for playback.
   * @param lastData Whether this is the last data we expect to receive for the
   *     initial
   * request.
   */
  static sendAudioDataInBuffers(
      audioData: Float32Array|null, sampleRate: number, bufferSize: number,
      timeInfo: ash.enhancedNetworkTts.mojom.TimingInfo[],
      sendTtsAudio: (audioBuffer: chrome.ttsEngine.AudioBuffer) => void,
      lastData: boolean): void {
    if (!audioData) {
      // TODO(crbug.com/40779585): Provide more appropriate error handling.
      return;
    }

    // The index in |timeInfo| that corresponds to the to-be-processed word.
    let timeInfoIdx = 0;
    // The index in |audioData| that corresponds to the start of the
    // to-be-processed word in |timeInfo|.
    let wordStartAtAudioData =
        Math.floor(parseFloat(timeInfo[0].timeOffset) * sampleRate);

    // Go through audioData and split it into AudioBuffers.
    for (let i = 0; i < audioData.length; i += bufferSize) {
      const audioBuffer = EnhancedNetworkTts.subarrayFrom(
          audioData, i /* startIdx */, bufferSize);

      // If the |wordStartAtAudioData| falls into this buffer, the buffer is
      // associated with a char index (text offset).
      let charIndex;
      if (i <= wordStartAtAudioData && i + bufferSize > wordStartAtAudioData) {
        charIndex = timeInfo[timeInfoIdx].textOffset;
        // Prepare for the next word.
        if (timeInfoIdx < timeInfo.length - 1) {
          timeInfoIdx++;
          wordStartAtAudioData = Math.floor(
              parseFloat(timeInfo[timeInfoIdx].timeOffset) * sampleRate);
        }
      }

      // Determine if the given buffer is the last buffer in the audioData.
      const isLastBuffer = lastData && (i + bufferSize >= audioData.length);
      sendTtsAudio({audioBuffer, charIndex, isLastBuffer});
    }
  }
}

TestImportManager.exportForTesting(EnhancedNetworkTts);
