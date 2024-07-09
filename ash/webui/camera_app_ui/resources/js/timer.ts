// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';

/**
 * A one-shot timer that is more powerful than setTimeout().
 */
export class OneShotTimer {
  private timeoutId: number|null = null;

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
    assert(this.timeoutId === null);
    this.timeoutId = setTimeout(this.handler, this.timeout);
  }

  /**
   * Stops the pending timeout. It's a no-op if the timer is already stopped.
   */
  stop(): void {
    if (this.timeoutId === null) {
      return;
    }
    clearTimeout(this.timeoutId);
    this.timeoutId = null;
  }

  /**
   * Resets the timer delay. It's a no-op if the timer is already stopped.
   */
  resetTimeout(): void {
    if (this.timeoutId === null) {
      return;
    }
    this.stop();
    this.start();
  }

  /**
   * Stops the timer and runs the scheduled handler immediately.
   */
  fireNow(): void {
    if (this.timeoutId !== null) {
      this.stop();
    }
    this.handler();
  }
}
