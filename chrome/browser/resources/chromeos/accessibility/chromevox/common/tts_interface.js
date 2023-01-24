// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a Tts interface.
 * All TTS engines in ChromeVox conform to the this interface.
 */
import {QueueMode, TtsSpeechProperties} from './tts_types.js';

/**
 * An interface for clients who want to get notified when an utterance
 * starts or ends from any source.
 * @interface
 */
export class TtsCapturingEventListener {
  /** Called when any utterance starts. */
  onTtsStart() {}

  /** Called when any utterance ends. */
  onTtsEnd() {}

  /** Called when any utterance gets interrupted. */
  onTtsInterrupted() {}
}

/** @interface */
export class TtsInterface {
  /**
   * Speaks the given string using the specified queueMode and properties.
   * @param {string} textString The string of text to be spoken.
   * @param {QueueMode} queueMode The queue mode to use for speaking.
   * @param {TtsSpeechProperties=} properties Speech properties to use for this
   *     utterance.
   * @return {TtsInterface} A tts object useful for chaining speak calls.
   */
  speak(textString, queueMode, properties) {}

  /**
   * Returns true if the TTS is currently speaking.
   * @return {boolean} True if the TTS is speaking.
   */
  isSpeaking() {}

  /**
   * Stops speech.
   */
  stop() {}

  /**
   * Adds a listener to get called whenever any utterance starts or ends.
   * @param {TtsCapturingEventListener} listener Listener to get called.
   */
  addCapturingEventListener(listener) {}

  /**
   * Removes a listener to get called whenever any utterance starts or ends.
   * @param {TtsCapturingEventListener} listener Listener to get called.
   */
  removeCapturingEventListener(listener) {}

  /**
   * Increases a TTS speech property.
   * @param {string} propertyName The name of the property to change.
   * @param {boolean} increase If true, increases the property value by one
   *     step size, otherwise decreases.
   */
  increaseOrDecreaseProperty(propertyName, increase) {}

  /**
   * Converts an engine property value to a percentage from 0.00 to 1.00.
   * @param {string} property The property to convert.
   * @return {?number} The percentage of the property.
   */
  propertyToPercentage(property) {}

  /**
   * Returns the default properties of the first tts that has default
   * properties.
   * @param {string} property Name of property.
   * @return {?number} The default value.
   */
  getDefaultProperty(property) {}

  /**
   * Toggles on or off speech.
   * @return {boolean} Whether speech is now on or off.
   */
  toggleSpeechOnOrOff() {}
}
