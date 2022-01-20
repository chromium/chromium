// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A waitable event for synchronization between asynchronous jobs.
 */
export class WaitableEvent<T = void> {
  private isSignaledInternal = false;
  // The field is definitely assigned in the constructor since the argument to
  // the Promise constructor is called immediately, but TypeScript can't
  // recognize that. Disable the check by adding "!" to the property name.
  private resolve!: (val: T) => void;
  private promise: Promise<T>;

  constructor() {
    this.promise = new Promise((resolve) => {
      this.resolve = resolve;
    });
  }

  /**
   * @return Whether the event is signaled
   */
  isSignaled(): boolean {
    return this.isSignaledInternal;
  }

  /**
   * Signals the event.
   */
  signal(value: T): void {
    if (this.isSignaledInternal) {
      return;
    }
    this.isSignaledInternal = true;
    this.resolve(value);
  }

  /**
   * @return Resolved when the event is signaled.
   */
  wait(): Promise<T> {
    return this.promise;
  }

  /**
   * @param timeout Timeout in ms.
   * @return Resolved when the event is signaled, or rejected when timed out.
   */
  timedWait(timeout: number): Promise<T> {
    const timeoutPromise = new Promise<T>((_resolve, reject) => {
      setTimeout(() => {
        reject(new Error(`Timed out after ${timeout}ms`));
      }, timeout);
    });
    return Promise.race([this.promise, timeoutPromise]);
  }
}
