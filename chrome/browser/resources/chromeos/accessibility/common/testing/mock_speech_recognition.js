// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * Mocking speech recognition for tests. Test classes can create a
 * new MockSpeechRecognition which will track any SpeechRecognizers created.
 */
class MockSpeechRecognition {
  /** @constructor */
  constructor() {
    /** @type {MockSpeechRecognizer} */
    this.speechRecognizer = null;
  }

  /**
   * Gets the speech recognizer, if one has been created.
   * @return {MockSpeechRecognizer}
   */
  getRecognizer() {
    return this.speechRecognizer;
  }

  /**
   * Calls the speech recognizer's onresult method.
   * @param {string} transcript
   * @param {boolean} isFinal
   */
  callOnResult(transcript, isFinal) {
    assertTrue(!!this.speechRecognizer);
    assertTrue(this.speechRecognizer.started);
    assertTrue(!!this.speechRecognizer.onresult);
    const event = {length: 1, resultIndex: 0, results: []};
    const result = [{transcript}];
    Object.defineProperty(result, 'isFinal', {
      get() {
        return isFinal;
      }
    });
    event.results.push(result);
    this.speechRecognizer.onresult(event);
  }

  /**
   * Calls the speech recognizer's onstart method.
   * @param {SpeechRecognitionErrorCode} error
   */
  callOnError(error) {
    assertTrue(!!this.speechRecognizer);
    assertTrue(this.speechRecognizer.started);
    assertTrue(!!this.speechRecognizer.onerror);
    this.speechRecognizer.onerror(error);
  }

  /** Calls the speech recognizer's onstart method. */
  callOnStart() {
    assertTrue(!!this.speechRecognizer);
    assertTrue(this.speechRecognizer.started);
    assertTrue(!!this.speechRecognizer.onstart);
    this.speechRecognizer.onstart();
  }

  /** Calls the speech recognizer's onend method. */
  callOnEnd() {
    assertTrue(!!this.speechRecognizer);
    assertTrue(!!this.speechRecognizer.onend);
    this.speechRecognizer.onend();
  }

  /**
   * Whether the MockSpeechRecognizer is started.
   * @return {boolean}
   */
  isStarted() {
    assertTrue(!!this.speechRecognizer);
    return this.speechRecognizer.started;
  }

  /**
   * The language set in the MockSpeechRecognizer.
   * @return {string}
   */
  lang() {
    assertTrue(!!this.speechRecognizer);
    return this.speechRecognizer.lang;
  }

  /**
   * Whether MockSpeechRecognizer should return continuous results.
   * @return {boolean}
   */
  continuous() {
    assertTrue(!!this.speechRecognizer);
    return this.speechRecognizer.continuous;
  }

  /**
   * Whether MockSpeechRecognizer should return interim results.
   * @return {boolean}
   */
  interimResults() {
    assertTrue(!!this.speechRecognizer);
    return this.speechRecognizer.interimResults;
  }
}

/**
 * A mock speech recognition engine for tests.
 * This class is a mock of webkitSpeechRecognition containing only the
 * functionality needed for Dictation to function.
 */
class MockSpeechRecognizer {
  /** @constructor */
  constructor() {
    /** @type {string} */
    this.lang = '';

    /** @type {boolean} */
    this.continuous = false;

    /** @type {boolean} */
    this.interimResults = false;

    /** @type {function<SpeechRecognitionResult>} */
    this.onresult;

    /** @type {function<>} */
    this.onstart;

    /** @type {function<>} */
    this.onend;

    /** @type {function<SpeechRecognitionErrorCode>} */
    this.onerror;

    /** @type {boolean} */
    this.started = false;

    window.mockSpeechRecognition.speechRecognizer = this;
  }

  /**
   * Starts mock speech recognition (in this case, just marks it as started).
   */
  start() {
    // Shouldn't call start twice.
    assertFalse(this.started);
    this.started = true;
  }

  /**
   * Ends mock speech recognition (in this case, just marks it as not started).
   */
  abort() {
    this.started = false;
  }
}
