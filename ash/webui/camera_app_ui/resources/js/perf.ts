// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';
import {reportError} from './error.js';
import * as expert from './expert.js';
import * as metrics from './metrics.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import * as state from './state.js';
import {
  ErrorLevel,
  ErrorType,
  Mode,
  PerfEntry,
  PerfEvent,
  PerfInformation,
} from './type.js';

type PerfEventListener = (perfEntry: PerfEntry) => void;

/**
 * The singleton instance of PerfLogger.
 *
 * Note that null means not initialized.
 */
let instance: PerfLogger|null = null;

/**
 * Logger for performance events.
 */
export class PerfLogger {
  /**
   * Map to store events starting timestamp.
   */
  private readonly startTimeMap = new Map<PerfEvent, number>();

  /**
   * Set of the listeners for perf events.
   */
  private readonly listeners = new Set<PerfEventListener>();

  /**
   * The timestamp when the measurement is interrupted.
   */
  private interruptedTime: number|null = null;

  /**
   * Returns the existing singleton instance of PerfLogger.
   */
  static getInstance(): PerfLogger {
    assert(instance !== null);
    return instance;
  }

  /**
   * Initialize `instance` before the usage. This should be called before
   * `getInstance()`.
   */
  static initializeInstance(): void {
    assert(instance === null);
    instance = new PerfLogger();
    // Setup listener for performance events.
    instance.addListener(async ({event, duration, perfInfo}) => {
      metrics.sendPerfEvent({event, duration, perfInfo});

      // Setup for console perf logger.
      if (expert.isEnabled(expert.ExpertOption.PRINT_PERFORMANCE_LOGS)) {
        // eslint-disable-next-line no-console
        console.log(
            '%c%s %s ms %s', 'color: #4E4F97; font-weight: bold;',
            event.padEnd(40), duration.toFixed(0).padStart(4),
            JSON.stringify(perfInfo));
      }

      // Setup for Tast tests logger.
      await window.appWindow?.reportPerf({event, duration, perfInfo});
    });

    state.addObserver(state.State.TAKING, (val, extras) => {
      // `taking` state indicates either taking photo or video. Skips for
      // some modes such as video mode since they didn't start `photo-taking`.
      if (!state.get(state.State.PHOTO_TAKING)) {
        return;
      }
      if (!val) {
        state.set(state.State.PHOTO_TAKING, false, extras);
      }
    });

    const states = Object.values(PerfEvent);
    for (const event of states) {
      state.addObserver(event, (val, extras) => {
        const perfLogger = PerfLogger.getInstance();
        if (val) {
          perfLogger.start(event);
        } else {
          perfLogger.stop(event, extras);
        }
      });
    }
  }

  /**
   * Adds listener for perf events.
   */
  addListener(listener: PerfEventListener): void {
    this.listeners.add(listener);
  }

  /**
   * Removes listener for perf events.
   *
   * @return Returns true if remove successfully. False otherwise.
   */
  removeListener(listener: PerfEventListener): boolean {
    return this.listeners.delete(listener);
  }

  /**
   * Starts the measurement for given event.
   *
   * @param event Target event.
   * @param startTime The start time of the event.
   */
  start(event: PerfEvent, startTime: number = performance.now()): void {
    if (this.startTimeMap.has(event)) {
      reportError(
          ErrorType.PERF_METRICS_FAILURE, ErrorLevel.ERROR,
          new Error(`Failed to start event ${
              event} since the previous one is not stopped.`));
      return;
    }
    this.startTimeMap.set(event, startTime);
    ChromeHelper.getInstance().startTracing(event);

    if (event === PerfEvent.PHOTO_CAPTURE_SHUTTER) {
      // `photo-taking` starts when a shutter button is clicked and finished
      // when a photo is saved. As scan mode has a review page in the middle,
      // returns early in this case.
      if (state.get(Mode.SCAN)) {
        return;
      }

      // If we set photo-taking state by 'taking' state, we cannot exclude
      // the timer duration. photo-capture-shutter is the timing that a
      // shutter is clicked.
      state.set(state.State.PHOTO_TAKING, true);
    }
  }

  /**
   * Stops the measurement for given event and returns the measurement result.
   *
   * @param event Target event.
   * @param perfInfo Optional information of this event for performance
   *     measurement.
   */
  stop(event: PerfEvent, perfInfo: PerfInformation = {}): void {
    if (!this.startTimeMap.has(event)) {
      reportError(
          ErrorType.PERF_METRICS_FAILURE, ErrorLevel.ERROR,
          new Error(`Failed to stop event ${event} which is never started.`));
      return;
    }

    const startTime = this.startTimeMap.get(event);
    assert(startTime !== undefined);
    this.startTimeMap.delete(event);

    // If there is error during performance measurement, drop it since it might
    // be inaccurate.
    if (perfInfo.hasError ?? false) {
      return;
    }

    // If the measurement is interrupted, drop the measurement since the result
    // might be inaccurate.
    if (this.interruptedTime !== null && startTime < this.interruptedTime) {
      // TODO(b/344473689): Currently, when entering review views after
      // capturing (e.g., GIF, Doc scan), the camera is suspended and
      // `reconfigure` is called repeatedly until the camera resumes. However,
      // this also interrupts performance events, preventing us from sending
      // perf events when the review view is opened. Force sending
      // DOCUMENT_PDF_SAVING for now.
      if (event !== PerfEvent.DOCUMENT_PDF_SAVING) {
        return;
      }
    }

    const duration = performance.now() - startTime;
    ChromeHelper.getInstance().stopTracing(event);
    for (const listener of this.listeners) {
      listener({event, duration, perfInfo});
    }
  }

  /**
   * Records the time of the interruption.
   */
  interrupt(): void {
    this.interruptedTime = performance.now();
  }
}
