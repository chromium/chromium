// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class OneShotTimer {
  private timerId: number|undefined;
  private promiseReject: ((reason?: any) => void)|undefined;

  constructor(private delayMs: number) {}

  // Cancels any running timer.
  reset(): void {
    if (this.timerId !== undefined) {
      clearTimeout(this.timerId);
      this.timerId = undefined;
      if (this.promiseReject) {
        this.promiseReject(new Error('Timer reset'));
        this.promiseReject = undefined;
      }
    }
  }

  // Cancels any running timer, starts a new one. Callback is only
  // run if the timer is not reset first.
  start(callback: () => void): void {
    this.startPromise().then(callback).catch(
        () => {
            // Catch and ignore timer reset.
        });
  }

  // Cancels any running timer, starts a new one. Resolves when
  // complete, rejects if canceled.
  startPromise(): Promise<void> {
    this.reset();
    return new Promise<void>((resolve, reject) => {
      this.promiseReject = reject;

      this.timerId = setTimeout(() => {
        this.timerId = undefined;
        resolve();
        this.promiseReject = undefined;
      }, this.delayMs);
    });
  }
}
