// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {recordAverageFps} from './metrics_utils.js';

/**
 * A class that keeps track of and records metrics related to FPS.
 */
export class PerformanceTracker {
  // The total number of frames that have been rendered.
  private totalFrameCount: number = 0;
  // The timestampe of the first frame supplied to the animation frame callback.
  private firstFrameTimestamp?: DOMHighResTimeStamp;
  // The timestamp of the last frame supplied to the animation frame callback.
  private lastFrameTimestamp?: DOMHighResTimeStamp;
  // The ID returned by requestAnimationFrame for the onAnimationFrame function.
  private onAnimationFrameRequestId?: number;

  reset() {
    this.totalFrameCount = 0;
    this.firstFrameTimestamp = undefined;
    this.lastFrameTimestamp = undefined;
    this.onAnimationFrameRequestId = undefined;
  }

  /**
   * Starts recording FPS metrics.
   */
  startSession() {
    // Ensure that startSession is only called once.
    assert(!this.onAnimationFrameRequestId);
    this.reset();

    // Start the FPS metrics session by requesting an animation frame.
    this.onAnimationFrameRequestId =
        requestAnimationFrame(this.onAnimationFrame.bind(this));
  }

  /**
   * Ends the FPS metrics session and records the performance metrics.
   */
  endSession() {
    if (!this.onAnimationFrameRequestId) {
      return;
    }
    cancelAnimationFrame(this.onAnimationFrameRequestId);
    this.onAnimationFrameRequestId = undefined;

    // Calculate and record the average FPS of the session.
    assert(this.firstFrameTimestamp && this.lastFrameTimestamp);
    const averageFrameTimeMs =
        (this.lastFrameTimestamp - this.firstFrameTimestamp) /
        this.totalFrameCount;
    const averageFps = 1000 / averageFrameTimeMs;
    recordAverageFps(averageFps);
  }

  private onAnimationFrame(timestamp: DOMHighResTimeStamp) {
    if (!this.firstFrameTimestamp) {
      this.firstFrameTimestamp = timestamp;
    }
    this.totalFrameCount++;
    this.lastFrameTimestamp = timestamp;

    this.onAnimationFrameRequestId =
        requestAnimationFrame(this.onAnimationFrame.bind(this));
  }
}
