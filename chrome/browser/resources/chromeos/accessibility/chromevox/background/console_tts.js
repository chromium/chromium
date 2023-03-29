// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A TTS engine that writes to globalThis.console.
 */
import {SpeechLog} from '../common/log_types.js';
import {QueueMode, TtsCategory} from '../common/tts_types.js';

import {LogStore} from './logging/log_store.js';
import {ChromeVoxPrefs} from './prefs.js';
import {TtsInterface} from './tts_interface.js';

/** @implements {TtsInterface} */
export class ConsoleTts {
  constructor() {
    /**
     * True if the console TTS is enabled by the user.
     * @private {boolean}
     */
    this.enabled_ = /** @type {boolean} */ (
        ChromeVoxPrefs.instance.getPrefs()['enableSpeechLogging']);
  }

  /**
   * @param {string} textString
   * @param {!QueueMode} queueMode
   * @param {Object=} properties
   * @return {!ConsoleTts}
   */
  speak(textString, queueMode, properties) {
    if (this.enabled_ && globalThis.console) {
      const category = properties?.category ?? TtsCategory.NAV;

      const speechLog = new SpeechLog(textString, queueMode, category);
      LogStore.instance.writeLog(speechLog);
      console.log(speechLog.toString());
    }
    return this;
  }

  /** @override */
  isSpeaking() {
    return false;
  }

  /** @override */
  stop() {
    if (this.enabled_) {
      console.log('Stop');
    }
  }

  /** @override */
  addCapturingEventListener(listener) {}

  /** @override */
  removeCapturingEventListener(listener) {}

  /** @override */
  increaseOrDecreaseProperty() {}

  /** @override */
  setProperty(propertyName, value) {}

  /**
   * @param {string} property
   * @return {number}
   * @override
   */
  propertyToPercentage(property) {}

  /**
   * Sets the enabled bit.
   * @param {boolean} enabled The new enabled bit.
   */
  setEnabled(enabled) {
    this.enabled_ = enabled;
  }

  /**
   * @param {string} property
   * @return {number}
   * @override
   */
  getDefaultProperty(property) {}

  /**
   * @return {boolean}
   * @override
   */
  toggleSpeechOnOrOff() {}
}
