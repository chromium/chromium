// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A composite TTS allows ChromeVox to use multiple TTS engines at
 * the same time.
 *
 */
import {TtsInterface} from '../common/tts_interface.js';
import {QueueMode, TtsSpeechProperties} from '../common/tts_types.js';

/**
 * A Composite Tts
 * @implements {TtsInterface}
 */
export class CompositeTts {
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
   * @return {!CompositeTts} this.
   */
  add(tts) {
    this.ttsEngines_.push(tts);
    return this;
  }

  /**
   * @param {string} textString
   * @param {QueueMode} queueMode
   * @param {TtsSpeechProperties=} properties
   * @return {TtsInterface}
   * @override
   */
  speak(textString, queueMode, properties) {
    this.ttsEngines_.forEach(
        engine => engine.speak(textString, queueMode, properties));
    return this;
  }

  /**
   * Returns true if any of the TTSes are still speaking.
   * @override
   */
  isSpeaking() {
    return this.ttsEngines_.some(engine => engine.isSpeaking());
  }

  /**
   * @override
   */
  stop() {
    this.ttsEngines_.forEach(engine => engine.stop());
  }

  /** @override */
  addCapturingEventListener(listener) {
    this.ttsEngines_.forEach(
        engine => engine.addCapturingEventListener(listener));
  }

  /** @override */
  removeCapturingEventListener(listener) {
    this.ttsEngines_.forEach(
        engine => engine.removeCapturingEventListener(listener));
  }

  /**
   * @override
   */
  increaseOrDecreaseProperty(propertyName, increase) {
    this.ttsEngines_.forEach(
        engine => engine.increaseOrDecreaseProperty(propertyName, increase));
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
    this.ttsEngines_.forEach(
        engine => value = value || engine.toggleSpeechOnOrOff());
    return value;
  }
}
