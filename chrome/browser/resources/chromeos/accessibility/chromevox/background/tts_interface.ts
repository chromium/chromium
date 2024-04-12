// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a Tts interface.
 * All TTS engines in ChromeVox conform to the this interface.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {QueueMode, TtsSpeechProperties} from '../common/tts_types.js';

/**
 * An interface for clients who want to get notified when an utterance
 * starts or ends from any source.
 */
export abstract class TtsCapturingEventListener {
  /** Called when any utterance starts. */
  abstract onTtsStart(): void;

  /** Called when any utterance ends. */
  abstract onTtsEnd(): void;

  /** Called when any utterance gets interrupted. */
  abstract onTtsInterrupted(): void;
}

/** @interface */
export abstract class TtsInterface {
  /**
   * Speaks the given string using the specified queueMode and properties.
   * @param textString The string of text to be spoken.
   * @param queueMode The queue mode to use for speaking.
   * @param properties Speech properties to use for this
   *     utterance.
   * @return A tts object useful for chaining speak calls.
   */
  abstract speak(
      textString: string, queueMode: QueueMode,
      properties?: TtsSpeechProperties): TtsInterface;

  /** @return True if the TTS is speaking. */
  abstract isSpeaking(): boolean;

  /** Stops speech. */
  abstract stop(): void;

  /** Adds a listener to get called whenever any utterance starts or ends. */
  abstract addCapturingEventListener(listener: TtsCapturingEventListener): void;

  /** Removes a listener to get called whenever any utterance starts or ends. */
  abstract removeCapturingEventListener(listener: TtsCapturingEventListener):
      void;

  /**
   * Increases a TTS speech property.
   * @param propertyName The name of the property to change.
   * @param increase If true, increases the property value by one
   *     step size, otherwise decreases.
   */
  abstract increaseOrDecreaseProperty(propertyName: string, increase: boolean):
      void;

  /**
   * Sets the property to a particular value. Callers should prefer this
   * to setting the underlying settings pref directly.
   * @param propertyName The name of the property to change.
   * @param value The value to change it to.
   */
  abstract setProperty(propertyName: string, value: number): void;

  /**
   * Converts an engine property value to a percentage from 0.00 to 1.00.
   * @param property The property to convert.
   * @return The percentage of the property.
   */
  abstract propertyToPercentage(property: string): number|null;

  /**
   * Returns the default properties of the first tts that has default
   * properties.
   * @param property Name of property.
   * @return The default value.
   */
  abstract getDefaultProperty(property: string): number|null;

  /**
   * Toggles on or off speech.
   * @return Whether speech is now on or off.
   */
  abstract toggleSpeechOnOrOff(): boolean;
}

TestImportManager.exportForTesting(TtsInterface);
