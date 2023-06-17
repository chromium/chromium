// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from '../../dom.js';
import {I18nString} from '../../i18n_string.js';
import * as loadTimeData from '../../models/load_time_data.js';
import {speak} from '../../spoken_msg.js';

/**
 * Maximal recording time in milliseconds and the function executed to notify
 * caller the tick reaches maximal time.
 */
export interface MaxTimeOption {
  maxTime: number;
  onMaxTimeout: () => void;
}

/**
 * Controller for the record-time of Camera view.
 */
abstract class RecordTimeBase {
  private readonly recordTime = dom.get('#record-time', HTMLElement);

  protected readonly maxTimeOption: MaxTimeOption|null = null;

  /**
   * Timeout to count every tick of elapsed recording time.
   */
  private tickTimeout: number|null = null;

  /**
   * Tick count of elapsed recording time.
   */
  private ticks = 0;

  /**
   * The timestamp when the recording starts.
   */
  private startTimestamp = 0;

  /**
   * The total duration of the recording in milliseconds.
   */
  private totalDuration = 0;

  /**
   * @return Time interval to update ticks in milliseconds.
   */
  protected abstract getTimeInterval(): number;

  /**
   * @param ticks Aggregated time ticks during the record time.
   * @return Message showing on record time area. Should already be translated
   *     by i18n if necessary.
   */
  protected abstract getTimeMessage(ticks: number): string;

  /**
   * Updates UI by the elapsed recording time.
   */
  private update() {
    dom.get('#record-time-msg', HTMLElement).textContent =
        this.getTimeMessage(this.ticks);
  }

  /**
   * Starts to count and show the elapsed recording time.
   *
   * @param resume Start parameters.
   * @param resume.resume If the time count is resumed from paused state.
   */
  start({resume}: {resume: boolean}): void {
    if (!resume) {
      this.ticks = 0;
      this.totalDuration = 0;
    }
    this.update();
    this.recordTime.hidden = false;

    this.tickTimeout = setInterval(() => {
      if (this.maxTimeOption === null ||
          (this.ticks + 1) * this.getTimeInterval() <=
              this.maxTimeOption.maxTime) {
        this.ticks++;
      } else {
        this.maxTimeOption.onMaxTimeout();
        if (this.tickTimeout !== null) {
          clearInterval(this.tickTimeout);
          this.tickTimeout = null;
        }
      }
      this.update();
    }, this.getTimeInterval());

    this.startTimestamp = performance.now();
  }

  /**
   * Stops counting and showing the elapsed recording time.
   *
   * @param pause Stop parameters.
   * @param pause.pause If the time count is paused temporarily.
   */
  stop({pause}: {pause: boolean}): void {
    speak(I18nString.STATUS_MSG_RECORDING_STOPPED);
    if (this.tickTimeout !== null) {
      clearInterval(this.tickTimeout);
      this.tickTimeout = null;
    }
    if (!pause) {
      this.ticks = 0;
      this.recordTime.hidden = true;
      this.update();
    }

    this.totalDuration += performance.now() - this.startTimestamp;
    if (this.maxTimeOption !== null) {
      this.totalDuration =
          Math.min(this.totalDuration, this.maxTimeOption.maxTime);
    }
  }

  /**
   * Returns the recorded duration in milliseconds.
   */
  inMilliseconds(): number {
    return Math.round(this.totalDuration);
  }
}

/**
 * Record time for normal record type.
 */
export class RecordTime extends RecordTimeBase {
  getTimeInterval(): number {
    return 1000;
  }

  getTimeMessage(ticks: number): string {
    // Format time into HH:MM:SS or MM:SS.
    function pad(n: number) {
      return (n < 10 ? '0' : '') + n;
    }
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
  declare protected readonly maxTimeOption: MaxTimeOption;

  constructor(maxTimeOption: MaxTimeOption) {
    super();
    this.maxTimeOption = maxTimeOption;
  }

  getTimeInterval(): number {
    return 100;
  }

  getTimeMessage(ticks: number): string {
    const maxTicks = this.maxTimeOption.maxTime / this.getTimeInterval();
    /**
     * Formats ticks to seconds with only first digit shown after floating
     * point.
     */
    const formatTick = (ticks: number): string =>
        (ticks / (1000 / this.getTimeInterval())).toFixed(1);
    return loadTimeData.getI18nMessage(
        I18nString.LABEL_CURRENT_AND_MAXIMAL_RECORD_TIME, formatTick(ticks),
        formatTick(maxTicks));
  }
}
