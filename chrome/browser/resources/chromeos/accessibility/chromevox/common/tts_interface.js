// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a Tts interface.
 *
 * All TTS engines in ChromeVox conform to the this interface.
 *
 */

/**
 * Categories for a speech utterance. This can be used with the
 * CATEGORY_FLUSH queue mode, which flushes all utterances from a given
 * category but not other utterances.
 *
 * NAV: speech related to explicit navigation, or focus changing.
 * LIVE: speech coming from changes to live regions.
 *
 * @enum {string}
 */
export const TtsCategory = {
  LIVE: 'live',
  NAV: 'nav',
};

/**
 * Queue modes for calls to {@code TtsInterface.speak}. The modes are listed in
 * descending order of priority.
 * @enum
 */
export const QueueMode = {
  /**
     Prepend the current utterance (if any) to the queue, stop speech, and
     speak this utterance.
   */
  INTERJECT: 0,

  /** Stop speech, clear everything, then speak this utterance. */
  FLUSH: 1,

  /**
   * Clear any utterances of the same category (as set by
   * properties['category']) from the queue, then enqueue this utterance.
   */
  CATEGORY_FLUSH: 2,

  /** Append this utterance to the end of the queue. */
  QUEUE: 3,
};

/**
 * An interface for clients who want to get notified when an utterance
 * starts or ends from any source.
 * @interface
 */
export class TtsCapturingEventListener {
  /**
   * Called when any utterance starts.
   */
  onTtsStart() {}

  /**
   * Called when any utterance ends.
   */
  onTtsEnd() {}

  /**
   * Called when any utterance gets interrupted.
   */
  onTtsInterrupted() {}
}

/** Structure to store properties around TTS speech production. */
export class TtsSpeechProperties {
  /** @param {Object=} opt_initialValues */
  constructor(opt_initialValues) {
    /** @public {TtsCategory|undefined} */
    this.category;

    /** @public {string|undefined} */
    this.color;

    /** @public {boolean|undefined} */
    this.delay;

    /** @public {boolean|undefined} */
    this.doNotInterrupt;

    /** @public {string|undefined} */
    this.fontWeight;

    /** @public {string|undefined} */
    this.lang;

    /** @public {boolean|undefined} */
    this.math;

    /** @public {boolean|undefined} */
    this.pause;

    /** @public {boolean|undefined} */
    this.phoneticCharacters;

    /** @public {string|undefined} */
    this.punctuationEcho;

    /** @public {boolean|undefined} */
    this.token;

    /** @public {string|undefined} */
    this.voiceName;

    /** @public {number|undefined} */
    this.pitch;
    /** @public {number|undefined} */
    this.relativePitch;

    /** @public {number|undefined} */
    this.rate;
    /** @public {number|undefined} */
    this.relativeRate;

    /** @public {number|undefined} */
    this.volume;
    /** @public {number|undefined} */
    this.relativeVolume;

    /** @public {function()|undefined} */
    this.startCallback;
    /** @public {function(boolean=)|undefined} */
    this.endCallback;

    /** @public {function(Object)|undefined} */
    this.onEvent;

    this.init_(opt_initialValues);
  }


  /** @return {!Object} */
  toJSON() {
    return Object.assign({}, this);
  }

  /**
   * @param {Object=} opt_initialValues
   * @private
   */
  init_(opt_initialValues) {
    if (!opt_initialValues) {
      return;
    }
    Object.assign(this, opt_initialValues);
  }
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

  /**
   * Sets the rate, pitch, and volume TTS Settings to their defaults.
   */
  resetTextToSpeechSettings() {}
}
