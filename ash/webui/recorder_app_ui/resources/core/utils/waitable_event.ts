// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A waitable event for synchronization between asynchronous jobs.
 */
export class WaitableEvent<T = void> {
  private isSignaledInternal = false;

  private readonly resolve: (val: PromiseLike<T>|T) => void;

  private readonly promise: Promise<T>;

  constructor() {
    const {promise, resolve} = Promise.withResolvers<T>();
    this.promise = promise;
    this.resolve = resolve;
  }

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
}
