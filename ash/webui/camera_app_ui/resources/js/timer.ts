// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';

/**
 * A one-shot timer that is more powerful than setTimeout().
 */
export class OneShotTimer {
  private timeoutId = 0;

  /**
   * The parameters are same as the parameters of setTimeout().
   */
  constructor(
      private readonly handler: () => void, private readonly timeout: number) {
    this.start();
  }

  /**
   * Starts the timer.
   */
  start(): void {
    assert(this.timeoutId === 0);
    this.timeoutId = setTimeout(this.handler, this.timeout);
  }

  /**
   * Stops the pending timeout.
   */
  stop(): void {
    assert(this.timeoutId !== 0);
    clearTimeout(this.timeoutId);
    this.timeoutId = 0;
  }

  /**
   * Resets the timer delay. It's a no-op if the timer is already stopped.
   */
  resetTimeout(): void {
    if (this.timeoutId === 0) {
      return;
    }
    this.stop();
    this.start();
  }

  /**
   * Stops the timer and runs the scheduled handler immediately.
   */
  fireNow(): void {
    if (this.timeoutId !== 0) {
      this.stop();
    }
    this.handler();
  }
}
