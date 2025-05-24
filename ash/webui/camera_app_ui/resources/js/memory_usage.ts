// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';
import {AsyncJobQueue} from './async_job_queue.js';
import {updateMemoryUsageEventDimensions} from './metrics.js';
import * as state from './state.js';
import {Mode} from './type.js';
import {measureUntrustedScriptsMemory} from './untrusted_scripts.js';

export interface CcaMemoryMeasurement {
  main: MemoryMeasurement;
  untrusted: MemoryMeasurement;
}

/**
 * Measures memory usage from trusted and untrusted frames.
 */
export async function measureAppMemoryUsage(): Promise<CcaMemoryMeasurement> {
  assert(self.crossOriginIsolated);
  const usages = await Promise.all([
    performance.measureUserAgentSpecificMemory(),
    measureUntrustedScriptsMemory(),
  ]);
  return {
    main: usages[0],
    untrusted: usages[1],
  };
}

const MEASUREMENT_INTERVAL_MS = 30000;

export enum SessionBehavior {
  TAKE_NORMAL_PHOTO = 1 << 0,
  TAKE_PORTRAIT_PHOTO = 1 << 1,
  SCAN_BARCODE = 1 << 2,
  SCAN_DOCUMENT = 1 << 3,
  RECORD_NORMAL_VIDEO = 1 << 4,
  RECORD_GIF_VIDEO = 1 << 5,
  RECORD_TIME_LAPSE_VIDEO = 1 << 6,
}

class MemoryMeasurementHelper {
  /**
   * A job queue for measuring memory usage.
   */
  private readonly jobQueue = new AsyncJobQueue('keepLatest');

  /**
   * Maximum memory usage in the current session, in bytes.
   */
  private maxUsage: number|null = null;

  /**
   * A number represented boolean bit flags for each |SessionBehavior|. The
   * value is updated in |measureWithSessionBehavior|.
   */
  private sessionBehavior = 0;

  constructor() {
    this.jobQueue.push(() => this.collectMemoryUsage());

    // Schedule the measurement every |MEASUREMENT_INTERVAL_MS| milliseconds.
    setInterval(() => {
      this.jobQueue.push(() => this.collectMemoryUsage());
    }, MEASUREMENT_INTERVAL_MS);

    // Measure memory usage when session behaviors are triggered.
    state.addEnabledStateObserver(state.State.TAKING, () => {
      if (state.get(Mode.PHOTO)) {
        this.measureWithSessionBehavior(SessionBehavior.TAKE_NORMAL_PHOTO);
      } else if (state.get(Mode.PORTRAIT)) {
        this.measureWithSessionBehavior(SessionBehavior.TAKE_PORTRAIT_PHOTO);
      }
    });

    const observeScanBehavior = () => {
      if (state.get(Mode.SCAN)) {
        if (state.get(state.State.ENABLE_SCAN_BARCODE)) {
          this.measureWithSessionBehavior(SessionBehavior.SCAN_BARCODE);
        } else if (state.get(state.State.ENABLE_SCAN_DOCUMENT)) {
          this.measureWithSessionBehavior(SessionBehavior.SCAN_DOCUMENT);
        }
      }
    };

    state.addEnabledStateObserver(Mode.SCAN, observeScanBehavior);
    state.addObserver(state.State.ENABLE_SCAN_BARCODE, observeScanBehavior);
    state.addObserver(state.State.ENABLE_SCAN_DOCUMENT, observeScanBehavior);

    state.addEnabledStateObserver(state.State.RECORDING, () => {
      if (state.get(state.State.RECORD_TYPE_NORMAL)) {
        this.measureWithSessionBehavior(SessionBehavior.RECORD_NORMAL_VIDEO);
      } else if (state.get(state.State.RECORD_TYPE_GIF)) {
        this.measureWithSessionBehavior(SessionBehavior.RECORD_GIF_VIDEO);
      } else if (state.get(state.State.RECORD_TYPE_TIME_LAPSE)) {
        this.measureWithSessionBehavior(
            SessionBehavior.RECORD_TIME_LAPSE_VIDEO);
      }
    });
  }

  private async collectMemoryUsage(): Promise<void> {
    const usage = await measureAppMemoryUsage();
    const totalUsage = usage.main.bytes + usage.untrusted.bytes;
    if (this.maxUsage === null || totalUsage > this.maxUsage) {
      this.maxUsage = totalUsage;
      updateMemoryUsageEventDimensions({
        memoryUsage: this.maxUsage,
        sessionBehavior: this.sessionBehavior,
      });
    }
  }

  private measureWithSessionBehavior(behavior: SessionBehavior): void {
    this.sessionBehavior |= behavior;
    this.jobQueue.push(() => this.collectMemoryUsage());
  }
}

let helper: MemoryMeasurementHelper|null = null;

/**
 * Starts scheduling memory measurement throughout the session.
 */
export function startMeasuringMemoryUsage(): void {
  if (helper === null) {
    helper = new MemoryMeasurementHelper();
  }
}
