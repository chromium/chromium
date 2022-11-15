// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Contains types related to speech generation.
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
