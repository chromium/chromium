// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A composite TTS allows ChromeVox to use multiple TTS engines at
 * the same time.
 */
import {QueueMode, TtsSpeechProperties} from '../common/tts_types.js';

import {TtsInterface} from './tts_interface.js';

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

  /** @override */
  increaseOrDecreaseProperty(propertyName, increase) {
    this.ttsEngines_.forEach(
        engine => engine.increaseOrDecreaseProperty(propertyName, increase));
  }

  /** @override */
  setProperty(propertyName, value) {
    this.ttsEngines_.forEach(engine => engine.setProperty(propertyName, value));
  }

  /** @override */
  propertyToPercentage(property) {
    const percentages =
        this.ttsEngines_.map(engine => engine.propertyToPercentage(property));
    // Return the first non-null percent, or null if all values are null.
    return percentages.find(percent => percent !== undefined) ?? null;
  }

  /** @override */
  getDefaultProperty(property) {
    const defaultValues =
        this.ttsEngines_.map(engine => engine.getDefaultProperty(property));
    // Return the first non-null value, or null if all values are null.
    return defaultValues.find(value => value !== null) ?? null;
  }

  /** @override */
  toggleSpeechOnOrOff() {
    let value = false;
    this.ttsEngines_.forEach(
        engine => value = value || engine.toggleSpeechOnOrOff());
    return value;
  }
}
