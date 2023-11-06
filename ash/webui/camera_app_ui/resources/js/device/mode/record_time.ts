// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from '../../dom.js';
import {I18nString} from '../../i18n_string.js';
import {RecordTimeChip} from '../../lit/components/record-time-chip.js';
import {getI18nMessage} from '../../models/load_time_data.js';
import {speak} from '../../spoken_msg.js';

/**
 * Time between updates in milliseconds.
 */
const UPDATE_INTERVAL_MS = 100;

/**
 * Controller for the record-time-chip of Camera view.
 */
export class RecordTime {
  private readonly recordTime = dom.get('record-time-chip', RecordTimeChip);

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
   * Maximal recording time in milliseconds.
   */
  private maxTimeMs: number|null = null;

  constructor(private readonly onMaxTimeout: () => void) {}

  private getTimeMessage(timeMs: number, maxTimeMs: number|null) {
    const seconds = timeMs / 1000;
    if (maxTimeMs === null) {
      // Normal recording. Format time into HH:MM:SS or MM:SS.
      const parts: number[] = [];
      if (seconds >= 3600) {
        parts.push(Math.floor(seconds / 3600));  // HH
      }
      parts.push(Math.floor(seconds / 60) % 60);  // MM
      parts.push(Math.floor(seconds % 60));       // SS
      return parts.map((n) => n.toString().padStart(2, '0')).join(':');
    } else {
      // GIF recording. Formats seconds with only first digit shown after
      // floating point.
      return getI18nMessage(
          I18nString.LABEL_CURRENT_AND_MAXIMAL_RECORD_TIME, seconds.toFixed(1),
          (maxTimeMs / 1000).toFixed(1));
    }
  }

  /**
   * Starts to count and show the elapsed recording time.
   */
  start(maxTimeMs: number|null = null): void {
    this.ticks = 0;
    this.totalDuration = 0;
    this.maxTimeMs = maxTimeMs;
    this.resume();
  }

  /**
   * Updates UI by the elapsed recording time.
   */
  private update() {
    this.recordTime.textContent =
        this.getTimeMessage(this.ticks * UPDATE_INTERVAL_MS, this.maxTimeMs);
  }

  /**
   * Resumes to count and show the elapsed recording time.
   */
  resume(): void {
    this.update();
    this.recordTime.hidden = false;

    this.tickTimeout = setInterval(() => {
      if (this.maxTimeMs === null ||
          (this.ticks + 1) * UPDATE_INTERVAL_MS <= this.maxTimeMs) {
        this.ticks++;
      } else {
        this.onMaxTimeout();
        if (this.tickTimeout !== null) {
          clearInterval(this.tickTimeout);
          this.tickTimeout = null;
        }
      }
      this.update();
    }, UPDATE_INTERVAL_MS);

    this.startTimestamp = performance.now();
  }

  /**
   * Calculates total duration after stop recoding.
   */
  private calculateDuration(): void {
    this.totalDuration += performance.now() - this.startTimestamp;
    if (this.maxTimeMs !== null) {
      this.totalDuration = Math.min(this.totalDuration, this.maxTimeMs);
    }
  }

  /**
   * Stops counting and showing the elapsed recording time.
   */
  stop(): void {
    speak(I18nString.STATUS_MSG_RECORDING_STOPPED);
    if (this.tickTimeout !== null) {
      clearInterval(this.tickTimeout);
      this.tickTimeout = null;
    }

    this.ticks = 0;
    this.recordTime.hidden = true;
    this.update();

    this.calculateDuration();
  }

  /**
   * Pauses counting and showing the elapsed recording time.
   */
  pause(): void {
    speak(I18nString.STATUS_MSG_RECORDING_STOPPED);
    if (this.tickTimeout !== null) {
      clearInterval(this.tickTimeout);
      this.tickTimeout = null;
    }

    this.calculateDuration();
  }

  /**
   * Returns the recorded duration in milliseconds.
   */
  inMilliseconds(): number {
    return Math.round(this.totalDuration);
  }
}
