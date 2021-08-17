// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class contains the main functionalities for Enhanced Network TTS.
 */
export class EnhancedNetworkTts {
  constructor() {}

  /**
   * Callback for onSpeakWithAudioStream event. This function responds to a TTS
   * request specified by the parameters |utterance|, |options|, and
   * |audioStreamOptions|. It uses the mojo API to safely retrieve audio that
   * fulfill the request and send the audio to |sendTtsAudio|.
   * @param {string} utterance The text to speak.
   * @param {!chrome.ttsEngine.SpeakOptions} options Options specified when a
   *     client calls the chrome.tts.speak.
   * @param {!chrome.ttsEngine.AudioStreamOptions} audioStreamOptions Contains
   *     the audio stream format expected to be produced by this engine.
   * @param {function(!chrome.ttsEngine.AudioBuffer): void} sendTtsAudio A
   *     function that accepts AudioBuffer for playback.
   * @param {function(string): void} sendError A function that signals error.
   */
  static async onSpeakWithAudioStreamEvent(
      utterance, options, audioStreamOptions, sendTtsAudio, sendError) {
    let api;
    try {
      api = /** @type {EnhancedNetworkTtsAdapter} */ (
          await chrome.mojoPrivate.requireAsync('ash.enhanced_network_tts'));
    } catch (e) {
      console.warn('Could not get mojoPrivate bindings: ' + e.message);
      // TODO(crbug.com/1231318): Provide more appropriate error handling.
      return;
    }

    const request = EnhancedNetworkTts.generateRequest(utterance, options);
    const processResponse = (response) => EnhancedNetworkTts.processResponse(
        response, audioStreamOptions, sendTtsAudio, sendError);
    await (api.getAudioDataWithCallback(request, processResponse));
  }

  /**
   * Callback for processing the |response| from the mojo API.
   * @param {!ash.enhancedNetworkTts.mojom.TtsResponse} response
   * @param {!chrome.ttsEngine.AudioStreamOptions} audioStreamOptions Contains
   *     the audio stream format expected to be produced by this engine.
   * @param {function(!chrome.ttsEngine.AudioBuffer): void} sendTtsAudio A
   *     function that accepts AudioBuffer for playback.
   * @param {function(string): void} sendError A function that signals error.
   */
  static async processResponse(
      response, audioStreamOptions, sendTtsAudio, sendError) {
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
        case ash.enhancedNetworkTts.mojom.TtsRequestError
            .kRequestOverride:  // not an error
          break;
      }
      return;
    }

    const lastData = response.data.lastData;
    const audioData = new Uint8Array(response.data.audio).buffer;
    const timeInfo = response.data.timeInfo;
    const decodedAudioData =
        await EnhancedNetworkTts.decodeAudioDataAtSampleRate(
            audioData, audioStreamOptions.sampleRate);

    EnhancedNetworkTts.sendAudioDataInBuffers(
        decodedAudioData, audioStreamOptions.sampleRate,
        audioStreamOptions.bufferSize, timeInfo, sendTtsAudio, lastData);
  }

  /**
   * Generates a request that can be used to query audio data from the Enhanced
   * Network TTS mojo API, which sends the request to the ReadAloud server.
   * @param {string} utterance The text to speak.
   * @param {!chrome.ttsEngine.SpeakOptions} options Options specified when a
   *     client calls the chrome.tts.speak.
   * @return {!ash.enhancedNetworkTts.mojom.TtsRequest}
   */
  static generateRequest(utterance, options) {
    // Gets the playback rate.
    const rate = options.rate || 1.0;

    // Unpack voice and lang. For lang, the server takes lang code only.
    let voice = options.voiceName || '';
    let lang = options.lang || '';
    lang = lang.trim().split(/_/)[0];

    // The ReadAloud server takes voice and lang as a pair. Sets them to
    // undefined if either is empty.
    if (voice.trim().length === 0 || lang.trim().length === 0) {
      voice = undefined;
      lang = undefined;
    }

    // The default voice is used for a situation where the user enables the
    // natural voices in Select-to-Speak but does not specify which voice name
    // to use. We override the |voice| and |lang| to |undefined| to get the
    // default voice from the server.
    if (voice === 'default-wavenet') {
      voice = undefined;
      lang = undefined;
    }

    return /** @type {!ash.enhancedNetworkTts.mojom.TtsRequest} */ (
        {utterance, rate, voice, lang});
  }

  /**
   * Decodes |inputAudioData| into a Float32Array at the |targetSampleRate|.
   * @param {!ArrayBuffer} inputAudioData The original data from audio files
   *     like mp3, ogg, or wav.
   * @param {number} targetSampleRate The sampling rate for decoding.
   * @return {Promise<Float32Array>}
   */
  static async decodeAudioDataAtSampleRate(inputAudioData, targetSampleRate) {
    const context = new AudioContext({sampleRate: targetSampleRate});

    let audioBuffer;
    try {
      audioBuffer = await context.decodeAudioData(inputAudioData);
    } catch (e) {
      console.warn('Could not decode audio data');
      return new Promise(resolve => resolve(null));
    }

    if (!audioBuffer) {
      return new Promise(resolve => resolve(null));
    }
    return audioBuffer.getChannelData(0);
  }

  /**
   * Creates a subarray from |startIdx| in the input |array|, with the size of
   * |subarraySize|.
   * @param {!Float32Array} array
   * @param {number} startIdx
   * @param {number} subarraySize
   * @return {!Float32Array}
   */
  static subarrayFrom(array, startIdx, subarraySize) {
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
   * @param {Float32Array} audioData Decoded audio data with a |sampleRate|.
   * @param {number} sampleRate
   * @param {number} bufferSize The size of each buffer that |sendTtsAudio|
   *     expects.
   * @param {!Array<!ash.enhancedNetworkTts.mojom.TimingInfo>} timeInfo An array
   *     of timestamp information for each word in the |audioData|.
   * @param {function(!chrome.ttsEngine.AudioBuffer): void} sendTtsAudio The
   *     function that takes |AudioBuffer| for playback.
   * @param {boolean} lastData Whether this is the last data we expect to
   * receive for the initial request.
   */
  static sendAudioDataInBuffers(
      audioData, sampleRate, bufferSize, timeInfo, sendTtsAudio, lastData) {
    if (!audioData) {
      // TODO(crbug.com/1231318): Provide more appropriate error handling.
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

  static async clearMojoRequests() {
    try {
      const api = /** @type {EnhancedNetworkTtsAdapter} */
          (await chrome.mojoPrivate.requireAsync('ash.enhanced_network_tts'));
      api.resetApi();
    } catch (e) {
      console.warn('Could not get mojoPrivate bindings: ' + e.message);
      // TODO(crbug.com/1231318): Provide more appropriate error handling.
      return;
    }
  }
}
