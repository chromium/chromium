// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A helper class for handling a single timer. Starting it will cancel any
 * previously pending timer it was managing. This is useful to avoid having to
 * manually keep track of any old running timers to cancel them when starting
 * a new one, when that behavior is desired.
 */
export class TimerHelper {
  // undefined means that no timer is running.
  private timerId_: undefined|number;

  // Much like window.setTimeout(), except that the first argument must be a
  // function, and it doesn't return anything, as the TimerHelper manages the
  // timer ID.Also stops any previously running timer managed by `this`.
  setTimeout = (func: Function, delay: number = 0, ...args: any[]) => {
    this.clearTimeout();
    this.timerId_ = setTimeout(() => {
      // Clear `timerId_`, to indicate no timer is running, and to avoid
      // cancelling a timer ID that could theoretically be reused for another
      // timer.
      this.timerId_ = undefined;
      func(...args);
    }, delay);
  };

  // Cancels the timer if running. Otherwise, does nothing.
  clearTimeout = () => {
    // This check is not strictly needed, since clearTimeout() does nothing when
    // passed undefined, but doesn't hurt.
    if (this.isRunning()) {
      clearTimeout(this.timerId_);
      this.timerId_ = undefined;
    }
  };

  // Returns true if the timer is currently running.
  isRunning = () => {
    return this.timerId_ !== undefined;
  };
}
