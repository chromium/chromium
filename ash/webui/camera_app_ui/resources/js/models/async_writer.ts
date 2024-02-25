// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../assert.js';
import {AsyncJobQueue} from '../async_job_queue.js';
import {Awaitable} from '../type.js';

/**
 * Represents a set of operations of a file-like writable stream. The seek and
 * close operations are optional.
 */
export interface AsyncOps {
  write(blob: Blob): Awaitable<void>;
  seek: ((offset: number) => Awaitable<void>)|null;
  close: (() => Promise<void>)|null;
}

/**
 * Asynchronous writer.
 */
export class AsyncWriter {
  private readonly queue = new AsyncJobQueue();

  private closed = false;

  constructor(private readonly ops: AsyncOps) {}

  /**
   * Checks whether the writer supports seek operation.
   */
  seekable(): boolean {
    return this.ops.seek !== null;
  }

  /**
   * Writes the |blob| asynchronously.
   */
  write(blob: Blob): void {
    assert(!this.closed);
    this.queue.push(() => this.ops.write(blob));
  }

  /**
   * Seeks to the specified |offset|.
   */
  seek(offset: number): void {
    assert(!this.closed);
    this.queue.push(async () => {
      assert(this.ops.seek !== null);
      await this.ops.seek(offset);
    });
  }

  /**
   * Closes the writer. No more write operations are allowed.
   *
   * @return Resolved when all write operations are finished.
   */
  async close(): Promise<void> {
    if (this.closed) {
      return;
    }
    this.closed = true;
    await this.queue
        .push(async () => {
          if (this.ops.close !== null) {
            await this.ops.close();
          }
        })
        .result;
  }

  /**
   * Combines multiple |writers| into one writer such that the blob would be
   * written to each of them.
   *
   * @return The combined writer.
   */
  static combine(...writers: AsyncWriter[]): AsyncWriter {
    function write(blob: Blob) {
      for (const writer of writers) {
        writer.write(blob);
      }
    }

    const allSeekable = writers.every((writer) => writer.seekable());
    function seekAll(offset: number) {
      for (const writer of writers) {
        writer.seek(offset);
      }
    }
    const seek = allSeekable ? seekAll : null;

    async function close() {
      await Promise.all(writers.map((writer) => writer.close()));
    }

    return new AsyncWriter({write, seek, close});
  }
}
