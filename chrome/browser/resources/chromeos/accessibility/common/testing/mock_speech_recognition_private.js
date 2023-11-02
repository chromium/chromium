// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @typedef {{transcript: string, isFinal: boolean}} */
let MockResultEvent;

/** @typedef {{message: string}} */
let MockErrorEvent;

/**
 * @typedef {{
 *   clientId: (number|undefined),
 *   locale: (string|undefined),
 *   interimResults: (boolean|undefined)
 * }}
 */
let MockStartOptions;

/**
 * @typedef {{
 *    clientId: (number|undefined)
 * }}
 */
let MockStopOptions;

/** @enum {string} */
const SpeechRecognitionType = {
  ON_DEVICE: 'onDevice',
  NETWORK: 'network',
};

/** A mock SpeechRecognitionPrivate API for tests. */
class MockSpeechRecognitionPrivate {
  /** @constructor */
  constructor() {
    // Properties.
    /** @private {boolean} */
    this.started_ = false;
    /** @private {!MockStartOptions}*/
    this.properties_ = {
      locale: undefined,
      interimResults: undefined,
    };
    /** @private {!SpeechRecognitionType} */
    this.speechRecognitionType_ = SpeechRecognitionType.NETWORK;

    // Event listeners.
    /** @private {?function({}):void} */
    this.onStopListener_ = null;
    /** @private {?function(!MockResultEvent):void} */
    this.onResultListener_ = null;
    /** @private {?function(!MockErrorEvent):void} */
    this.onErrorListener_ = null;

    // Mock events.

    /**
     * @type {!{
     *  addListener: function(Function):void
     *  removeListener: function(Function):void}}
     */
    this.onStop = {
      addListener: listener => {
        this.onStopListener_ = listener;
      },
      removeListener: listener => {
        if (this.onStopListener_ === listener) {
          this.onStopListener_ = null;
        }
      },
    };

    /**
     * @type {!{
     *  addListener: function(Function):void
     *  removeListener: function(Function):void}}
     */
    this.onResult = {
      addListener: listener => {
        this.onResultListener_ = listener;
      },
      removeListener: listener => {
        if (this.onResultListener_ === listener) {
          this.onResultListener_ = null;
        }
      },
    };

    /**
     * @type {!{
     *  addListener: function(Function):void
     *  removeListener: function(Function):void}}
     */
    this.onError = {
      addListener: listener => {
        this.onErrorListener_ = listener;
      },
      removeListener: listener => {
        if (this.onErrorListener_ === listener) {
          this.onErrorListener_ = null;
        }
      },
    };
  }

  // Mock methods.

  /**
   * @param {!MockStartOptions} props
   * @param {function(SpeechRecognitionType): void} callback
   */
  start(props, callback) {
    chrome.runtime.lastError = null;
    if (this.started_) {
      // If speech recognition is already active when calling start(), the real
      // API will set chrome.runtime.lastError. Do the same for the mock API.
      chrome.runtime.lastError = {
        message: 'Speech recognition already started',
      };
    }

    this.started_ = true;

    // The real API will update its properties when start() is called. Only
    // update properties that are specified by |props|.
    // Ignore `clientId`, since Dictation is the only client of this API.
    this.properties_.locale =
        props.locale !== undefined ? props.locale : this.properties_.locale;
    this.properties_.interimResults = props.interimResults !== undefined ?
        props.interimResults :
        this.properties_.interimResults;

    callback(this.speechRecognitionType_);
  }

  /**
   * @param {!MockStopOptions} props
   * @param {function():void} callback
   */
  stop(props, callback) {
    chrome.runtime.lastError = null;
    if (!this.started_) {
      // If speech recognition is already inactive when calling stop(), the real
      // API will set chrome.runtime.lastError. Do the same for the mock API.
      chrome.runtime.lastError = {
        message: 'Speech recognition already stopped',
      };
    }

    // The real API will run the callback and send an onStop event if speech
    // recognition was stopped by the API call.
    callback();
    if (this.started_) {
      this.fireMockStopEvent();
    }
  }

  // Methods for firing fake events.

  /**
   * @param {string} transcript
   * @param {boolean} isFinal
   */
  fireMockOnResultEvent(transcript, isFinal) {
    assertTrue(
        this.started_,
        'Speech recognition should be active when firing a result event');
    assertTrue(
        Boolean(this.onResultListener_),
        'Client should have added an onResult listener');

    // The real API will fire an onResult event.
    this.onResultListener_({transcript, isFinal});
  }

  /** Fires a fake stop event. */
  fireMockStopEvent() {
    assertTrue(
        this.started_,
        'Speech recognition should be active when firing a stop event');
    assertTrue(
        Boolean(this.onStopListener_),
        'Client should have added an onStop listener');

    // The real API will turn off speech recognition and fire an onStop event.
    this.started_ = false;
    this.onStopListener_({});
  }

  /** Fires a fake error event. */
  fireMockOnErrorEvent() {
    assertTrue(
        this.started_,
        'Speech recognition should be active when firing an error event');
    assertTrue(
        Boolean(this.onErrorListener_),
        'Client should have added an onError listener');

    // The real API will fire an onError and an onStop event.
    this.fireMockStopEvent();
    this.onErrorListener_({message: 'Speech recognition error'});
  }

  // Miscellaneous and helper methods.

  /**
   * The APIs properties are updated whenever start() is called. Updates
   * properties by calling start(), then stop().
   * @param {!MockStartOptions} props
   */
  updateProperties(props) {
    this.start(props, () => {});
    this.stop({}, () => {});
  }

  /** @return {boolean} */
  isStarted() {
    return this.started_ === true;
  }

  /** @return {string} */
  locale() {
    return this.properties_.locale;
  }

  /** @return {boolean} */
  interimResults() {
    return this.properties_.interimResults;
  }

  /** @param {!SpeechRecognitionType} type */
  setSpeechRecognitionType(type) {
    this.speechRecognitionType_ = type;
  }
}
