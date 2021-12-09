// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '../../../assert.js';
import * as dom from '../../../dom.js';
import {I18nString} from '../../../i18n_string.js';
import * as loadTimeData from '../../../models/load_time_data.js';
import {speak} from '../../../toast.js';

/**
 * Maximal recording time in milliseconds and the function executed to notify
 * caller the tick reaches maximal time.
 * @typedef {{
 *   maxTime: number,
 *   onMaxTimeout: (function(): *),
 * }}
 */
export let MaxTimeOption;

/**
 * Controller for the record-time of Camera view.
 * @abstract
 */
class RecordTimeBase {
  /**
   * @param {MaxTimeOption=} maxTimeOption
   * @public
   */
  constructor(maxTimeOption) {
    /**
     * @type {!HTMLElement}
     * @private
     */
    this.recordTime_ = dom.get('#record-time', HTMLElement);

    /**
     * @const {?MaxTimeOption}
     * @protected
     */
    this.maxTimeOption_ = maxTimeOption || null;

    /**
     * Timeout to count every tick of elapsed recording time.
     * @type {?number}
     * @private
     */
    this.tickTimeout_ = null;

    /**
     * Tick count of elapsed recording time.
     * @type {number}
     * @private
     */
    this.ticks_ = 0;

    /**
     * The timestamp when the recording starts.
     * @type {number}
     * @private
     */
    this.startTimestamp_ = 0;

    /**
     * The total duration of the recording in milliseconds.
     * @type {number}
     * @private
     */
    this.totalDuration_ = 0;
  }

  /**
   * @return {number} Time interval to update ticks in milliseconds.
   * @protected
   * @abstract
   */
  getTimeInterval_() {
    assertNotReached();
  }

  /**
   * @param {number} ticks Aggregated time ticks during the record time.
   * @return {string} Message showing on record time area. Should already be
   *     translated by i18n if necessary.
   * @protected
   * @abstract
   */
  getTimeMessage_(ticks) {
    assertNotReached();
  }

  /**
   * Updates UI by the elapsed recording time.
   * @private
   */
  update_() {
    dom.get('#record-time-msg', HTMLElement).textContent =
        this.getTimeMessage_(this.ticks_);
  }

  /**
   * Starts to count and show the elapsed recording time.
   * @param {{resume: boolean}} params If the time count is resumed from paused
   *     state.
   */
  start({resume}) {
    if (!resume) {
      this.ticks_ = 0;
      this.totalDuration_ = 0;
    }
    this.update_();
    this.recordTime_.hidden = false;

    this.tickTimeout_ = setInterval(() => {
      if (this.maxTimeOption_ === null ||
          (this.ticks_ + 1) * this.getTimeInterval_() <=
              this.maxTimeOption_.maxTime) {
        this.ticks_++;
      } else {
        this.maxTimeOption_.onMaxTimeout();
        clearInterval(this.tickTimeout_);
        this.tickTimeout_ = null;
      }
      this.update_();
    }, this.getTimeInterval_());

    this.startTimestamp_ = performance.now();
  }

  /**
   * Stops counting and showing the elapsed recording time.
   * @param {{pause: boolean}} param If the time count is paused temporarily.
   */
  stop({pause}) {
    speak(I18nString.STATUS_MSG_RECORDING_STOPPED);
    if (this.tickTimeout_) {
      clearInterval(this.tickTimeout_);
      this.tickTimeout_ = null;
    }
    if (!pause) {
      this.ticks_ = 0;
      this.recordTime_.hidden = true;
      this.update_();
    }

    this.totalDuration_ += performance.now() - this.startTimestamp_;
    if (this.maxTimeOption_ !== null) {
      this.totalDuration_ =
          Math.min(this.totalDuration_, this.maxTimeOption_.maxTime);
    }
  }

  /**
   * Returns the recorded duration in milliseconds.
   * @return {number}
   */
  inMilliseconds() {
    return this.totalDuration_;
  }
}

/**
 * Record time for normal record type.
 */
export class RecordTime extends RecordTimeBase {
  /**
   * @override
   */
  getTimeInterval_() {
    return 1000;
  }

  /**
   * @override
   */
  getTimeMessage_(ticks) {
    // Format time into HH:MM:SS or MM:SS.
    const pad = (n) => {
      return (n < 10 ? '0' : '') + n;
    };
    let hh = '';
    if (ticks >= 3600) {
      hh = pad(Math.floor(ticks / 3600)) + ':';
    }
    const mm = pad(Math.floor(ticks / 60) % 60) + ':';
    return hh + mm + pad(ticks % 60);
  }
}

/**
 * Record time for gif record type.
 */
export class GifRecordTime extends RecordTimeBase {
  /**
   * @override
   */
  getTimeInterval_() {
    return 100;
  }

  /**
   * @override
   */
  getTimeMessage_(ticks) {
    const maxTicks = this.maxTimeOption_.maxTime / this.getTimeInterval_();
    /**
     * Formats ticks to seconds with only first digit shown after floating
     * point.
     * @param {number} ticks
     * @return {string} Formatted string.
     */
    const formatTick = (ticks) =>
        (ticks / (1000 / this.getTimeInterval_())).toFixed(1);
    return loadTimeData.getI18nMessage(
        I18nString.LABEL_CURRENT_AND_MAXIMAL_RECORD_TIME, formatTick(ticks),
        formatTick(maxTicks));
  }
}
