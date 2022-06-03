// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A composite TTS allows ChromeVox to use multiple TTS engines at
 * the same time.
 *
 */

goog.provide('CompositeTts');

goog.require('TtsInterface');

/**
 * A Composite Tts
 * @implements {TtsInterface}
 */
CompositeTts = class {
  constructor() {
    /**
     * @type {Array<TtsInterface>}
     * @private
     */
    this.ttsEngines_ = [];
  }

  /**
   * Adds a TTS engine to the composite TTS
   * @param {TtsInterface} tts The TTS to add.
   * @return {CompositeTts} this.
   */
  add(tts) {
    this.ttsEngines_.push(tts);
    return this;
  }

  /**
   * @override
   */
  speak(textString, queueMode, properties) {
    this.ttsEngines_.forEach(function(engine) {
      engine.speak(textString, queueMode, properties);
    });
    return this;
  }

  /**
   * Returns true if any of the TTSes are still speaking.
   * @override
   */
  isSpeaking() {
    return this.ttsEngines_.some(function(engine) {
      return engine.isSpeaking();
    });
  }

  /**
   * @override
   */
  stop() {
    this.ttsEngines_.forEach(function(engine) {
      engine.stop();
    });
  }

  /** @override */
  addCapturingEventListener(listener) {
    this.ttsEngines_.forEach(function(engine) {
      engine.addCapturingEventListener(listener);
    });
  }

  /** @override */
  removeCapturingEventListener(listener) {
    this.ttsEngines_.forEach(function(engine) {
      engine.removeCapturingEventListener(listener);
    });
  }

  /**
   * @override
   */
  increaseOrDecreaseProperty(propertyName, increase) {
    this.ttsEngines_.forEach(function(engine) {
      engine.increaseOrDecreaseProperty(propertyName, increase);
    });
  }

  /**
   * @override
   */
  propertyToPercentage(property) {
    for (let i = 0, engine; engine = this.ttsEngines_[i]; i++) {
      const value = engine.propertyToPercentage(property);
      if (value !== undefined) {
        return value;
      }
    }
    return null;
  }

  /**
   * @override
   */
  getDefaultProperty(property) {
    for (let i = 0, engine; engine = this.ttsEngines_[i]; i++) {
      const value = engine.getDefaultProperty(property);
      if (value !== undefined) {
        return value;
      }
    }
    return null;
  }

  /** @override */
  toggleSpeechOnOrOff() {
    let value = false;
    this.ttsEngines_.forEach(function(engine) {
      value = value || engine.toggleSpeechOnOrOff();
    });
    return value;
  }

  /** @override */
  resetTextToSpeechSettings() {
    this.ttsEngines_.forEach(function(engine) {
      engine.resetTextToSpeechSettings();
    });
  }
};
