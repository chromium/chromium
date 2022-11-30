// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * A mocked chrome.mojoPrivate API for tests.
 */
const MockMojoPrivate = {
  /**
   * The map for all the registered modules.
   * @private
   * @dict
   */
  registeredModule_: {},

  // Methods from chrome.mojoPrivate API. //

  /**
   * Returns a promise that will resolve to an asynchronously loaded module.
   * @param {string} moduleName
   * @return {*}
   */
  async requireAsync(moduleName) {
    if (moduleName in this.registeredModule_) {
      return new Promise(
          resolve => resolve(this.registeredModule_[moduleName]));
    }
    return Promise.reject(
        new Error('no matched module in the mocked mojoPrivate API.'));
  },

  // Methods for testing. //

  /**
   * Empty out all the registered modules.
   */
  unregisterAllModuleForTesting() {
    this.registeredModule_ = {};
  },

  /**
   * Registers a mocked module into chrome.mojoPrivate API.
   * @param {string} moduleName
   * @param {*} module
   */
  registerMockedModuleForTesting(moduleName, module) {
    this.registeredModule_[moduleName] = module;
  },
};

/*
 * A mocked ash.enhanced_network_tts API for tests.
 */
const MockTtsApi = {

  /**
   * The queued results to be returned to clients calling |getAudioData|. Each
   * call will return one result from the head of the queue.
   * @type {Array<!Promise<{response:
   *     !ash.enhancedNetworkTts.mojom.TtsResponse}>>}
   * @private
   */
  queuedResults_: [],

  // Methods from ash.enhanced_network_tts API. //

  /**
   * Gets the audio data for the input request.
   * @param {!ash.enhancedNetworkTts.mojom.TtsRequest} request
   * @param {function(!ash.enhancedNetworkTts.mojom.TtsResponse)} callback a
   *     function that receives TtsResponse.
   * @return {!Promise<void>}
   */
  getAudioDataWithCallback(request, callback) {
    if (this.queuedResults_.length === 0) {
      // This should never happen but in case future developers accidentally
      // trigger it.
      console.warn(
          'Should not call a mocked API without specifying expected data.');
      return Promise.reject(new Error('Empty data.'));
    }
    callback(this.queuedResults_.shift());
    return new Promise(resolve => resolve());
  },

  // Methods for testing. //
  /**
   * Enqueues a response with an error code to the |queuedResults_|.
   * @param {number} errorCode
   */
  enqueueErrorCode(errorCode) {
    const response = {errorCode};
    this.queuedResults_.push(response);
  },

  /**
   * Enqueues a response with audio data to the |queuedResults_|.
   * @param {!Array<number>} audio
   * @param {!Array<!ash.enhancedNetworkTts.mojom.TimingInfo>} timeInfo
   * @param {boolean} lastData
   */
  enqueueAudioData(audio, timeInfo, lastData) {
    const response = {'data': {audio, timeInfo, lastData}};
    this.queuedResults_.push(response);
  },
};

/**
 * Mock ash constants. This should be kept synchronized with
 * //chromeos/ash/components/enhanced_network_tts/enhanced_network_tts.mojom-lite.js.
 */
const ash = {};
ash.enhancedNetworkTts = {};
ash.enhancedNetworkTts.mojom = {};
ash.enhancedNetworkTts.mojom.TtsRequestError = {
  kEmptyUtterance: 0,
  kOverLength: 1,
  kServerError: 2,
  kReceivedUnexpectedData: 3,
  kRequestOverride: 4,
  MIN_VALUE: 0,
  MAX_VALUE: 4,
};
