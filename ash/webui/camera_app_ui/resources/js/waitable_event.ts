// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A waitable event for synchronization between asynchronous jobs.
 */
export class WaitableEvent<T = void> {
  private isSignaledInternal = false;
  private resolve: (val: T) => void;
  private promise: Promise<T>;
  /**
   * @public
   */
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
  signal(value: T|undefined): void {
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
