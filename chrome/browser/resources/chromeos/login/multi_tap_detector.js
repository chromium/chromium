// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Multi-tap gesture detector for web UI OOBE.
 */


// #import {assert} from 'chrome://resources/js/assert.m.js';

/** Multi-tap gesture detector. */
/* #export */ class MultiTapDetector {
  /**
   * @param {?HTMLElement} element UI element to attach the multi-tap detector to.
   * @param {number} tapsCount Number of taps in multi-tap gesture to detect.
   * @param {!function()} callback Callback to be called when multi-tap gesture
   *     is detected.
   */
  constructor(element, tapsCount, callback) {
    assert(callback);
    assert(tapsCount > 0);
    assert(element);

    /** @private {number} */
    this.tapsSeen_ = 0;

    /** @private {?Date} */
    this.lastTapTime_ = null;

    this.callback_ = callback;
    this.tapsCount_ = tapsCount;

    element.addEventListener('click', this.onTap_.bind(this));
  }

  /**
   * Returns current time or fake time for testing if set.
   * @return {!Date}
   * @private
   */
  getCurrentTime_() {
    return MultiTapDetector.FAKE_TIME_FOR_TESTS ?
        MultiTapDetector.FAKE_TIME_FOR_TESTS :
        new Date();
  }

  /**
   * Handles tap event.
   * @private
   */
  onTap_() {
    let timestamp = this.getCurrentTime_();
    if (!this.lastTapTime_ ||
        timestamp - this.lastTapTime_ <
            MultiTapDetector.IN_BETWEEN_TAPS_TIME_MS) {
      this.tapsSeen_++;
      if (this.tapsSeen_ >= this.tapsCount_) {
        this.tapsSeen_ = 0;
        this.callback_();
      }
    } else {
      this.tapsSeen_ = 0;
    }
    this.lastTapTime_ = timestamp;
  }
}

/**
 * Time in between taps used to recognize multi-tap gesture.
 * @const {number}
 */
MultiTapDetector.IN_BETWEEN_TAPS_TIME_MS = 400;

/**
 * Fake time used for testing. If set it will be used instead of the current
 * time.
 * @const {?Date}
 */
MultiTapDetector.FAKE_TIME_FOR_TESTS = null;
