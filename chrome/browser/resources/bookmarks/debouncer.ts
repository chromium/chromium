// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import type {TimerProxy} from './types.js';

/**
 * @fileoverview A debouncer which fires the given callback after a delay. The
 * delay can be refreshed by calling restartTimeout. Resetting the timeout with
 * no delay moves the callback to the end of the task queue.
 */

export class Debouncer {
  private callback_: () => void;
  private timer_: number|null = null;
  private timerProxy_: TimerProxy;
  private boundTimerCallback_: () => void;
  private isDone_: boolean = false;
  private promiseResolver_: PromiseResolver<void>;

  constructor(callback: () => void) {
    this.callback_ = callback;
    this.timerProxy_ = window;
    this.boundTimerCallback_ = this.timerCallback_.bind(this);
    this.promiseResolver_ = new PromiseResolver();
  }

  /**
   * Starts the timer for the callback, cancelling the old timer if there is
   * one.
   */
  restartTimeout(delay?: number) {
    assert(!this.isDone_);

    this.cancelTimeout_();
    this.timer_ =
        this.timerProxy_.setTimeout(this.boundTimerCallback_, delay || 0);
  }

  done(): boolean {
    return this.isDone_;
  }

  get promise(): Promise<void> {
    return this.promiseResolver_.promise;
  }

  /**
   * Resets the debouncer as if it had been newly instantiated.
   */
  reset() {
    this.isDone_ = false;
    this.promiseResolver_ = new PromiseResolver();
    this.cancelTimeout_();
  }

  /**
   * Cancel the timer callback, which can be restarted by calling
   * restartTimeout().
   */
  private cancelTimeout_() {
    if (this.timer_) {
      this.timerProxy_.clearTimeout(this.timer_);
    }
  }

  private timerCallback_() {
    this.isDone_ = true;
    this.callback_.call(this);
    this.promiseResolver_.resolve();
  }
}
