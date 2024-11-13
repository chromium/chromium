// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FacialGesture} from './facial_gestures.js';

export class GestureTimer {
  private minDurationMs_ = GestureTimer.DEFAULT_MINIMUM_DURATION_MS;
  private gestureStart_: Map<FacialGesture, Date> = new Map();
  private useGestureDuration_ = true;

  /**
   * Mark that the gesture has been detected at the given time stamp. If the
   * gesture has already been marked as started then this timestamp is ignored.
   */
  mark(gesture: FacialGesture, timestamp: Date): void {
    if (!this.gestureStart_.has(gesture)) {
      this.gestureStart_.set(gesture, timestamp);
    }
  }

  /** Reset the timer for the given gesture. */
  reset(gesture: FacialGesture): void {
    this.gestureStart_.delete(gesture);
  }

  /** Reset the timer for all gestures. */
  resetAll(): void {
    this.gestureStart_.clear();
  }

  /**
   * Return true if this gesture has been held for a valid duration in relation
   * to the given timestamp.
   */
  isDurationValid(gesture: FacialGesture, timestamp: Date): boolean {
    if (!this.useGestureDuration_) {
      return true;
    }

    const startTime = this.gestureStart_.get(gesture);
    // If there is no start timestamp, then the duration is not valid.
    if (!startTime) {
      return false;
    }

    return timestamp.getTime() - startTime.getTime() > this.minDurationMs_;
  }

  // For testing purposes, we want to allow gestures to be recognized instantly
  // without requiring a valid duration.
  setGestureDurationForTesting(useDuration: boolean): void {
    this.useGestureDuration_ = useDuration;
  }
}

export namespace GestureTimer {
  /** Minimum time duration for a gesture to be recognized. */
  export const DEFAULT_MINIMUM_DURATION_MS = 150;
}
