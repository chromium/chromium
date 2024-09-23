// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Multi-tap gesture detector for web UI OOBE.
 */

import {assert} from '//resources/js/assert.js';

/** Multi-tap gesture detector. */
export class MultiTapDetector {
  private tapsSeen: number;
  private lastTapTime: Date|null;
  private inBetweenTapsTimeMs: number;
  private callback: () => void;
  private tapsCount: number;

  private static fakeTimeForTests: Date;

  /**
   * element UI element to attach the multi-tap detector to.
   * tapsCount Number of taps in multi-tap gesture to detect.
   * callback Callback to be called when multi-tap gesture is detected.
   */
  constructor(element: HTMLElement, tapsCount: number, callback: () => void) {
    assert(tapsCount > 0);

    this.tapsSeen = 0;
    this.lastTapTime = null;

    /**
     * Time in between taps used to recognize multi-tap gesture.
     */
    this.inBetweenTapsTimeMs = 400;

    this.callback = callback;
    this.tapsCount = tapsCount;

    element.addEventListener('click', this.onTap.bind(this));
  }

  /**
   * TODO(crbug.com/1319450) - Use a proper static variable
   * Sets a fake time to be used during testing.
   */
  static setFakeTimeForTests(fakeTime: Date): void {
    MultiTapDetector.fakeTimeForTests = fakeTime;
  }

  /**
   * Returns current time or fake time for testing if set.
   */
  private getCurrentTime(): Date {
    return MultiTapDetector.fakeTimeForTests ?
        MultiTapDetector.fakeTimeForTests :
        new Date();
  }

  /**
   * Handles tap event.
   */
  private onTap(): void {
    const timestamp = this.getCurrentTime();
    if (!this.lastTapTime ||
        (timestamp.getTime() - this.lastTapTime.getTime() <
         this.inBetweenTapsTimeMs)) {
      this.tapsSeen++;
      if (this.tapsSeen >= this.tapsCount) {
        this.tapsSeen = 0;
        this.callback();
      }
    } else {
      this.tapsSeen = 0;
    }
    this.lastTapTime = timestamp;
  }
}
