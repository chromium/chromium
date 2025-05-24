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
   * The timestamp when the recording starts.
   */
  private startTimestamp: number|null = null;

  /**
   * The total duration of the recording in milliseconds.
   */
  private totalDuration = 0;

  /**
   * Maximal recording time in milliseconds.
   */
  private maxTimeMs = Infinity;

  constructor(private readonly onMaxTimeout: () => void) {}

  private getTimeMessage(timeMs: number) {
    const seconds = timeMs / 1000;
    if (Number.isFinite(this.maxTimeMs)) {
      // GIF recording. Formats seconds with only first digit shown after
      // floating point.
      return getI18nMessage(
          I18nString.LABEL_CURRENT_AND_MAXIMAL_RECORD_TIME, seconds.toFixed(1),
          (this.maxTimeMs / 1000).toFixed(1));
    } else {
      // Normal recording. Format time into HH:MM:SS or MM:SS.
      const parts: number[] = [];
      if (seconds >= 3600) {
        parts.push(Math.floor(seconds / 3600));  // HH
      }
      parts.push(Math.floor(seconds / 60) % 60);  // MM
      parts.push(Math.floor(seconds % 60));       // SS
      return parts.map((n) => n.toString().padStart(2, '0')).join(':');
    }
  }

  /**
   * Starts to count and show the elapsed recording time.
   */
  start(maxTimeMs = Infinity): void {
    this.totalDuration = 0;
    this.maxTimeMs = maxTimeMs;
    this.resume();
  }

  /**
   * Updates UI by the elapsed recording time.
   */
  private update() {
    this.recordTime.textContent = this.getTimeMessage(this.getDuration());
  }

  /**
   * Gets the current duration of the recording.
   */
  private getDuration() {
    if (this.startTimestamp === null) {
      return this.totalDuration;
    }
    return this.totalDuration + performance.now() - this.startTimestamp;
  }

  /**
   * Resumes to count and show the elapsed recording time.
   */
  resume(): void {
    this.startTimestamp = performance.now();
    this.update();
    this.recordTime.hidden = false;

    this.tickTimeout = setInterval(() => {
      if (this.getDuration() > this.maxTimeMs) {
        this.onMaxTimeout();
        if (this.tickTimeout !== null) {
          clearInterval(this.tickTimeout);
          this.tickTimeout = null;
        }
      }
      this.update();
    }, UPDATE_INTERVAL_MS);
  }

  /**
   * Calculates total duration after recoding is stopped or paused.
   */
  private accumulateTotalDuration(): void {
    if (this.startTimestamp !== null) {
      this.totalDuration += performance.now() - this.startTimestamp;
      this.startTimestamp = null;
    }
    this.totalDuration = Math.min(this.totalDuration, this.maxTimeMs);
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

    this.recordTime.hidden = true;
    this.accumulateTotalDuration();
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
    this.accumulateTotalDuration();
  }

  /**
   * Returns the recorded duration in milliseconds.
   */
  inMilliseconds(): number {
    return Math.round(this.getDuration());
  }
}
