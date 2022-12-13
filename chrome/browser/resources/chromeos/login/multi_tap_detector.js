// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Multi-tap gesture detector for web UI OOBE.
 */

import {assert} from 'chrome://resources/ash/common/assert.js';

/** Multi-tap gesture detector. */
export class MultiTapDetector {
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

    /**
     * Time in between taps used to recognize multi-tap gesture.
     * @const {number}
     */
    this.inBetweenTapsTimeMs_ = 400;

    this.callback_ = callback;
    this.tapsCount_ = tapsCount;

    element.addEventListener('click', this.onTap_.bind(this));
  }

  /**
   * TODO(crbug.com/1319450) - Use a proper static variable
   * Sets a fake time to be used during testing.
   * @param {Date} fakeTime
   */
  static setFakeTimeForTests(fakeTime) {
    MultiTapDetector.FAKE_TIME_FOR_TESTS = fakeTime;
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
    const timestamp = this.getCurrentTime_();
    if (!this.lastTapTime_ ||
        timestamp - this.lastTapTime_ < this.inBetweenTapsTimeMs_) {
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
