// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '../../assert.js';
import {AsyncJobQueue} from '../../async_job_queue.js';
import * as comlink from '../../lib/comlink.js';
import runFfmpeg from '../../lib/ffmpeg.js';
import {WaitableEvent} from '../../waitable_event.js';
import {AsyncWriter} from '../async_writer.js';

import {VideoProcessorArgs} from './video_processor_args.js';

/**
 * A file stream in Emscripten.
 */
interface FileStream {
  position: number;
}

/**
 * The set of callbacks for an emulated device in Emscripten. Ref:
 * https://emscripten.org/docs/api_reference/Filesystem-API.html#FS.registerDevice.
 */
interface FileOps {
  open(stream: FileStream): void;
  close(stream: FileStream): void;
  read(
      stream: FileStream, buffer: Int8Array, offset: number, length: number,
      position: number): number;
  write(
      stream: FileStream, buffer: Int8Array, offset: number, length: number,
      position?: number): number;
  llseek(stream: FileStream, offset: number, whence: number): number;
}

type ReadableCallback = (deviceReadable: number) => void;

/*
 * Emscripten FileSystem API. This is the minimal type definitions that we're
 * using here.
 *
 * Ref: https://emscripten.org/docs/api_reference/Filesystem-API.html#devices
 */
interface FsStream {
  fd: number;
}
interface Fs {
  makedev(major: number, minor: number): number;
  mkdev(path: string, mode?: number): void;
  registerDevice(dev: number, ops: FileOps): void;
  symlink(oldpath: string, newpath: string): void;
  open(path: string, flags: string): FsStream;
}

/**
 * Handle ExitStatus from emscripten_force_exit when stop recording.
 * TODO(b/199980849): Find build options or function to handle FFMpeg to not
 * throw ExitStatus on normal stopping.
 */
function exitNormally(err: unknown) {
  if (err instanceof Object && 'name' in err && 'status' in err) {
    return err.name === 'ExitStatus' && err.status === 0;
  }
  return false;
}

/**
 * An emulated input device backed by Int8Array.
 */
class InputDevice {
  /**
   * The data to be read from the device.
   */
  private readonly data: Int8Array[] = [];

  /**
   * Whether the writing is canceled.
   */
  private isCanceled = false;

  /**
   * Whether the writing is ended. If true, no more data would be pushed.
   */
  private ended = false;

  /**
   * The callback to be triggered when the device is ready to read(). The
   * callback would be called only once and reset to null afterward. It should
   * be called with 1 if the device is readable or 0 if it is canceled.
   */
  private readableCallback: ReadableCallback|null = null;

  /**
   * Returns 1 if the device is readable or 0 if it is canceled.
   */
  private isDeviceReadable(): number {
    return this.isCanceled ? 0 : 1;
  }

  /**
   * Notifies and resets the readable callback, if any.
   */
  consumeReadableCallback(): void {
    if (this.readableCallback === null) {
      return;
    }
    const callback = this.readableCallback;
    this.readableCallback = null;
    callback(this.isDeviceReadable());
  }

  /**
   * Pushes a chunk of data into the device.
   */
  push(data: Int8Array): void {
    assert(!this.ended);
    this.data.push(data);
    this.consumeReadableCallback();
  }

  /**
   * Closes the writing pipe.
   */
  endPush(): void {
    this.ended = true;
    try {
      this.consumeReadableCallback();
    } catch (e) {
      if (!exitNormally(e)) {
        throw e;
      }
    }
  }

  /**
   * Implements the read() operation for the emulated device.
   *
   * @param stream The target stream.
   * @param buffer The destination buffer.
   * @param offset The destination buffer offset.
   * @param length The maximum length to read.
   * @param position The position to read from stream.
   * @return The numbers of bytes read.
   */
  read(
      stream: FileStream, buffer: Int8Array, offset: number, length: number,
      position: number): number {
    assert(position === stream.position, 'stdin is not seekable');
    if (this.data.length === 0) {
      assert(this.ended);
      return 0;
    }

    let bytesRead = 0;
    while (this.data.length > 0 && length > 0) {
      const data = this.data[0];
      const len = Math.min(data.length, length);
      buffer.set(data.subarray(0, len), offset);
      if (len === data.length) {
        this.data.shift();
      } else {
        this.data[0] = data.subarray(len);
      }

      offset += len;
      length -= len;
      bytesRead += len;
    }

    return bytesRead;
  }

  getFileOps(): FileOps {
    return {
      open: () => {
          // Do nothing.
      },
      close: () => {
          // Do nothing.
      },
      read: (...args) => this.read(...args),
      write: () => assertNotReached('write should not be called on stdin'),
      llseek: () => assertNotReached('llseek should not be called on stdin'),
    };
  }

  /**
   * Sets the readable callback. The callback would be called immediately if
   * the device is in a readable state.
   */
  setReadableCallback(callback: ReadableCallback) {
    if (this.data.length > 0 || this.ended) {
      callback(this.isDeviceReadable());
      return;
    }
    assert(this.readableCallback === null);
    this.readableCallback = callback;
  }

  /**
   * Marks the input device as canceled so that ffmpeg can handle it properly.
   */
  cancel() {
    this.isCanceled = true;
  }
}

/**
 * An emulated output device.
 */
class OutputDevice {
  private readonly closed = new WaitableEvent();

  /**
   * @param output Where should the device write to.
   */
  constructor(private readonly output: AsyncWriter) {}

  /**
   * Implements the write() operation for the emulated device.
   *
   * @param stream The target stream.
   * @param buffer The source buffer.
   * @param offset The source buffer offset.
   * @param length The maximum length to be write.
   * @param position The position to write in stream.
   * @return The numbers of bytes written.
   */
  write(
      stream: FileStream, buffer: Int8Array, offset: number, length: number,
      position?: number): number {
    assert(!this.closed.isSignaled());
    const blob = new Blob([buffer.subarray(offset, offset + length)]);
    assert(
        position === undefined || position === stream.position,
        'combined seek-and-write operation is not supported');
    this.output.write(blob);
    return length;
  }

  /**
   * Implements the llseek() operation for the emulated device.
   * Only SEEK_SET (0) is supported as |whence|. Reference:
   * https://emscripten.org/docs/api_reference/Filesystem-API.html#FS.llseek.
   *
   * @param stream The target stream.
   * @param offset The offset in bytes relative to |whence|.
   * @param whence The reference position to be used.
   * @return The resulting file position.
   */
  llseek(stream: FileStream, offset: number, whence: number): number {
    assert(whence === 0, 'only SEEK_SET is supported');
    assert(this.output.seekable());
    if (stream.position !== offset) {
      this.output.seek(offset);
    }
    return offset;
  }

  /**
   * Implements the close() operation for the emulated device.
   */
  close(): void {
    this.closed.signal();
  }

  /**
   * @return Resolved when the device is closed.
   */
  async waitClosed(): Promise<void> {
    await this.closed.wait();
  }

  getFileOps(): FileOps {
    return {
      open: () => {
          // Do nothing.
      },
      close: () => this.close(),
      read: () => assertNotReached('read should not be called on output'),
      write: (...args) => this.write(...args),
      llseek: (...args) => this.llseek(...args),
    };
  }
}

/**
 * A ffmpeg-based video processor that can process input and output data
 * incrementally.
 */
class FfmpegVideoProcessor {
  private readonly inputDevice = new InputDevice();

  private readonly outputDevice: OutputDevice;

  private readonly jobQueue = new AsyncJobQueue();

  /**
   * @param output The output writer.
   */
  constructor(
      private readonly output: AsyncWriter, processorArgs: VideoProcessorArgs) {
    this.outputDevice = new OutputDevice(output);

    const outputFile = `/output.${processorArgs.outputExtension}`;

    // clang-format formats one argument per line, which makes the list harder
    // to read with comments.
    // clang-format off
    const args = [
      // Make the procssing pipeline start earlier by shorten the initial
      // analyze duration from the default 5s to 1s. This reduce the
      // stop-capture latency significantly for short videos.
      '-analyzeduration', '1M',
      // input from stdin
      ...processorArgs.decoderArgs, '-i', 'pipe:0',
      // arguments for output format encoder
      ...processorArgs.encoderArgs,
      // show error log only
      '-hide_banner', '-loglevel', 'error',
      // do not ask anything
      '-nostdin', '-y',
      // output to file
      outputFile,
    ];
    // clang-format on

    const config = {
      arguments: args,
      locateFile: (file: string) => {
        assert(file === 'ffmpeg.wasm');
        // util.expandPath can't be used here since util includes
        // load_time_data, which includes file under chrome://, but this file
        // is in chrome-untrusted://.
        // TODO(pihsun): Separate util into multiple files so we can include
        // expandPath here.
        // TODO(b/213408699): Separate files included in different scope
        // (chrome://, chrome-untrusted://, worker) into different folder /
        // tsconfig.json, so this can be caught at compile time.
        return '../../../js/lib/ffmpeg.wasm';
      },
      // This is from emscripten.
      // eslint-disable-next-line @typescript-eslint/naming-convention
      noFSInit: true,  // It would be setup in preRun().
      preRun: [() => {
        // The FS property are injected by emscripten at runtime.
        /* eslint-disable-next-line
             @typescript-eslint/naming-convention,
             @typescript-eslint/consistent-type-assertions */
        const fs = (config as unknown as {FS: Fs}).FS;
        assert(fs !== null);
        // 80 is just a random major number that won't collide with other
        // default devices of the Emscripten runtime environment, which uses
        // major numbers 1, 3, 5, 6, 64, and 65. Ref:
        // https://github.com/emscripten-core/emscripten/blob/1ed6dd5cfb88d927ec03ecac8756f0273810d5c9/src/library_fs.js#L1331
        const input = fs.makedev(80, 0);
        fs.registerDevice(input, this.inputDevice.getFileOps());
        fs.mkdev('/dev/stdin', input);

        const output = fs.makedev(80, 1);
        fs.registerDevice(output, this.outputDevice.getFileOps());
        fs.mkdev(outputFile, output);

        fs.symlink('/dev/tty1', '/dev/stdout');
        fs.symlink('/dev/tty1', '/dev/stderr');
        const stdin = fs.open('/dev/stdin', 'r');
        const stdout = fs.open('/dev/stdout', 'w');
        const stderr = fs.open('/dev/stderr', 'w');
        assert(stdin.fd === 0);
        assert(stdout.fd === 1);
        assert(stderr.fd === 2);
      }],
      waitReadable: (callback: ReadableCallback) => {
        this.inputDevice.setReadableCallback(callback);
      },
    };

    function initFfmpeg() {
      return new Promise<void>((resolve) => {
        // runFFmpeg() is a special function exposed by Emscripten that will
        // return an object with then(). The function passed into then() would
        // be called when the runtime is initialized. Note that because the
        // then() function will return the object itself again, using await here
        // would cause an infinite loop.
        void runFfmpeg(config).then(() => resolve());
      });
    }
    this.jobQueue.push(initFfmpeg);
  }

  /**
   * Writes a blob with mkv data into the processor.
   */
  write(blob: Blob): void {
    this.jobQueue.push(async () => {
      const buf = await blob.arrayBuffer();
      this.inputDevice.push(new Int8Array(buf));
    });
  }

  /**
   * Closes the writer. No more write operations are allowed.
   *
   * @return Resolved when all write operations are finished.
   */
  async close(): Promise<void> {
    // Flush and close the input device.
    this.jobQueue.push(() => {
      this.inputDevice.endPush();
    });
    await this.jobQueue.flush();

    // Wait until the output device is closed.
    await this.outputDevice.waitClosed();

    // Flush and close the output writer.
    await this.output.close();
  }

  /**
   * Cancels all the remaining tasks and notifies ffmpeg that the writing is
   * canceled.
   */
  async cancel(): Promise<void> {
    // Clear and make sure there is no pending task.
    await this.jobQueue.clear();
    this.jobQueue.push(() => {
      this.inputDevice.cancel();
      // When input device is cancelled, for some reason calling
      // emscripten_force_exit() will not close the corresponding file
      // descriptor. As a result, we explicitly close it here.
      this.outputDevice.close();
    });
    await this.close();
  }
}

// Only export types to ensure that the file is not imported by other files at
// runtime.
export type VideoProcessorConstructor = typeof FfmpegVideoProcessor;
export type VideoProcessor = FfmpegVideoProcessor;

/**
 * Expose the VideoProcessor constructor to given end point.
 */
function exposeVideoProcessor(endPoint: MessagePort) {
  comlink.expose(FfmpegVideoProcessor, endPoint);
}

comlink.expose({exposeVideoProcessor});
