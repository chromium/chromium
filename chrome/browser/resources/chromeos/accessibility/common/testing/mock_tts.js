// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * A mock text-to-speech engine for tests.
 * This class has functions and callbacks necessary for accessibility extensions
 * (Select-to-Speak, ChromeVox) to function. It keeps track of the utterances
 * currently being spoken, and whether TTS should be speaking or is stopped.
 * @constructor
 */
var MockTts = function() {
  /**
   * @type {Array<string>}
   * @private
   */
  this.pendingUtterances_ = [];

  /**
   * @type {boolean}
   * @private
   */
  this.currentlySpeaking_ = false;

  /**
   * A list of callbacks to call each time speech is requested.
   * These are stored such that the last one should be called
   * first. Each should only be used once.
   * @type {Array<function(string)>}
   * @private
   */
  this.speechCallbackStack_ = [];

  /**
   * Options object for speech.
   * @type {onEvent: !function({type: string, charIndex: number})}
   * @private
   */
  this.options_ = null;

  /**
   * Whether TTS events will be queued instead of sent immediately.
   * @private {boolean}
   */
  this.waitToSendEvents_ = false;

  /**
   * TTS engine events waiting to be sent. Each entry in the array is an array
   * of two elements: [options, event], where |options| is the TTS options
   * during the time of that event, and |event| is the event isself.
   * @private {!Array<!Array<!Object>>}
   */
  this.pendingEvents_ = [];

  /**
   * @enum {string}
   * @see https://developer.chrome.com/extensions/tts#type-EventType
   */
  this.EventType = chrome.tts.EventType;
};

MockTts.prototype = {
  // Functions based on methods in
  // https://developer.chrome.com/extensions/tts
  speak(utterance, options) {
    this.pendingUtterances_.push(utterance);
    this.currentlySpeaking_ = true;
    if (options && options.onEvent) {
      this.options_ = options;
      this.sendEvent({type: this.EventType.START, charIndex: 0});
    }
    if (this.speechCallbackStack_.length > 0) {
      this.speechCallbackStack_.pop()(utterance);
    }
  },
  finishPendingUtterance() {
    this.pendingUtterances_ = [];
    this.currentlySpeaking_ = false;
    if (this.options_) {
      this.sendEvent({type: this.EventType.END});
    }
  },
  /**
   * Mock the speaking process of TTS, and simulate a word end event.
   * @param {number} nextStartIndex The start char index of the word to be
   *     spoken.
   */
  speakUntilCharIndex(nextStartIndex) {
    this.currentlySpeaking_ = true;
    if (this.options_) {
      this.sendEvent({type: this.EventType.WORD, charIndex: nextStartIndex});
    }
  },
  stop() {
    this.pendingUtterances_ = [];
    this.currentlySpeaking_ = false;
    if (this.options_) {
      this.sendEvent({type: this.EventType.INTERRUPTED});
      this.options_ = null;
    }
  },
  getVoices(callback) {
    callback([{
      voiceName: 'English US',
      lang: 'en-US',
      eventTypes: [
        this.EventType.START,
        this.EventTypeEND,
        this.EventType.WORD,
        this.EventType.CANCELLED,
      ],
    }]);
  },
  isSpeaking(callback) {
    callback(this.currentlySpeaking_);
  },
  // Functions for testing
  currentlySpeaking() {
    return this.currentlySpeaking_;
  },
  pendingUtterances() {
    return this.pendingUtterances_;
  },
  setOnSpeechCallbacks(callbacks) {
    this.speechCallbackStack_ = callbacks.reverse();
  },
  getOptions() {
    return this.options_;
  },
  sendEvent(event) {
    if (!this.options_) {
      return;
    }
    if (this.waitToSendEvents_) {
      // Queue event if set to wait on events.
      this.pendingEvents_.push([this.options_, event]);
      return;
    }
    this.options_.onEvent(event);
  },
  setWaitToSendEvents(waitToSendEvents) {
    this.waitToSendEvents_ = waitToSendEvents;
  },
  sendPendingEvents() {
    while (this.pendingEvents_.length > 0) {
      const [options, event] = this.pendingEvents_.pop();
      options.onEvent(event);
    }
  },
};
