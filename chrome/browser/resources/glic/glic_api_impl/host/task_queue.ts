// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A queue for running async tasks in serial.
export class TaskQueue {
  private queue:
      Array<{task: () => Promise<any>, promise: PromiseWithResolvers<any>}> =
          [];
  private running = false;

  // Adds a task to the queue. Returns a promise that will be resolved when the
  // task is completes.
  add<T>(task: () => Promise<T>): Promise<T> {
    const promise = Promise.withResolvers<T>();
    this.queue.push({task, promise});
    this.runTasks();
    return promise.promise;
  }

  private async runTasks(): Promise<void> {
    if (this.running) {
      return;
    }
    this.running = true;
    while (this.queue.length > 0) {
      const {task, promise} = this.queue.shift()!;
      try {
        const result = await task();
        promise.resolve(result);
      } catch (e) {
        promise.reject(e);
      }
    }
    this.running = false;
  }
}
