// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

/**
 * @fileoverview A debouncer which fires the given callback after a delay. The
 * delay can be refreshed by calling restartTimeout. Resetting the timeout with
 * no delay moves the callback to the end of the task queue.
 */

export class Debouncer {
  /** @param {!function()} callback */
  constructor(callback) {
    /** @private {!function()} */
    this.callback_ = callback;
    /** @private {!Object} */
    this.timerProxy_ = window;
    /** @private {?number} */
    this.timer_ = null;
    /** @private {!function()} */
    this.boundTimerCallback_ = this.timerCallback_.bind(this);
    /** @private {boolean} */
    this.isDone_ = false;
    /** @private {!PromiseResolver} */
    this.promiseResolver_ = new PromiseResolver();
  }

  /**
   * Starts the timer for the callback, cancelling the old timer if there is
   * one.
   * @param {number=} delay
   */
  restartTimeout(delay) {
    assert(!this.isDone_);

    this.cancelTimeout_();
    this.timer_ =
        this.timerProxy_.setTimeout(this.boundTimerCallback_, delay || 0);
  }

  /**
   * @return {boolean} True if the Debouncer has finished processing.
   */
  done() {
    return this.isDone_;
  }

  /**
   * @return {!Promise} Promise which resolves immediately after the callback.
   */
  get promise() {
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
   * @private
   */
  cancelTimeout_() {
    if (this.timer_) {
      this.timerProxy_.clearTimeout(this.timer_);
    }
  }

  /** @private */
  timerCallback_() {
    this.isDone_ = true;
    this.callback_.call();
    this.promiseResolver_.resolve();
  }
}
