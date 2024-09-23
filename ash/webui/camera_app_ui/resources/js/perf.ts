// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from './assert.js';
import {reportError} from './error.js';
import * as expert from './expert.js';
import * as metrics from './metrics.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import * as state from './state.js';
import {
  ErrorLevel,
  ErrorType,
  PerfEntry,
  PerfEvent,
  PerfInformation,
  Pressure,
} from './type.js';

type PerfEventListener = (perfEntry: PerfEntry) => void;

interface PerfEventValue {
  startTime: number;
  pressure?: Pressure;
}

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
   * Map to store events starting timestamp and the most pressure.
   */
  private readonly perfEventMap = new Map<PerfEvent, PerfEventValue>();

  /**
   * Set of the listeners for perf events.
   */
  private readonly listeners = new Set<PerfEventListener>();

  /**
   * The timestamp when the measurement is interrupted.
   */
  private interruptedTime: number|null = null;

  private pendingEvents: PerfEntry[] = [];

  private readonly observer: PressureObserver;

  private previousPressure: Pressure|null = null;

  constructor() {
    this.observer = new PressureObserver((records: PressureRecord[]) => {
      const latestPressureState = records[records.length - 1].state;
      const latestPressure = this.stateToPressure(latestPressureState);
      if (this.pendingEvents.length > 0) {
        for (const pendingEvent of this.pendingEvents) {
          pendingEvent.perfInfo.pressure = latestPressure;
          for (const listener of this.listeners) {
            listener(pendingEvent);
          }
        }
        this.pendingEvents = [];
      }
      for (const eventValue of this.perfEventMap.values()) {
        if (eventValue.pressure === undefined ||
            eventValue.pressure < latestPressure) {
          eventValue.pressure = latestPressure;
        }
      }
      this.previousPressure = latestPressure;
    }, {sampleInterval: 1000});
    this.observer.observe('cpu');
  }

  /**
   * Returns Pressure enum from string `state`.
   */
  private stateToPressure(state: string): Pressure {
    switch (state) {
      case 'nominal':
        return Pressure.NOMINAL;
      case 'fair':
        return Pressure.FAIR;
      case 'serious':
        return Pressure.SERIOUS;
      case 'critical':
        return Pressure.CRITICAL;
      default:
        assertNotReached('Unexpected pressure');
    }
  }

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
    if (this.perfEventMap.has(event)) {
      reportError(
          ErrorType.PERF_METRICS_FAILURE, ErrorLevel.ERROR,
          new Error(`Failed to start event ${
              event} since the previous one is not stopped.`));
      return;
    }
    this.perfEventMap.set(event, {startTime});
    ChromeHelper.getInstance().startTracing(event);
  }

  /**
   * Stops the measurement for given event and returns the measurement result.
   *
   * @param event Target event.
   * @param perfInfo Optional information of this event for performance
   *     measurement.
   */
  stop(event: PerfEvent, perfInfo: PerfInformation = {}): void {
    if (!this.perfEventMap.has(event)) {
      reportError(
          ErrorType.PERF_METRICS_FAILURE, ErrorLevel.ERROR,
          new Error(`Failed to stop event ${event} which is never started.`));
      return;
    }

    const perfEventVal = this.perfEventMap.get(event);
    assert(perfEventVal !== undefined);
    this.perfEventMap.delete(event);

    // If there is error during performance measurement, drop it since it might
    // be inaccurate.
    if (perfInfo.hasError ?? false) {
      return;
    }

    perfInfo.pressure = perfEventVal.pressure;

    // If the measurement is interrupted, drop the measurement since the result
    // might be inaccurate.
    if (this.interruptedTime !== null &&
        perfEventVal.startTime < this.interruptedTime) {
      return;
    }

    const duration = performance.now() - perfEventVal.startTime;
    ChromeHelper.getInstance().stopTracing(event);

    if (this.previousPressure === null) {
      this.pendingEvents.push({event, duration, perfInfo});
      return;
    }
    if (perfInfo.pressure === undefined) {
      perfInfo.pressure = this.previousPressure;
    }
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
