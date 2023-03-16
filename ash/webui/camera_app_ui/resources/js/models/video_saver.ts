// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../assert.js';
import {Intent} from '../intent.js';
import * as Comlink from '../lib/comlink.js';
import {
  MimeType,
  Resolution,
} from '../type.js';
import {getVideoProcessorHelper} from '../untrusted_scripts.js';

import {AsyncWriter} from './async_writer.js';
import {
  VideoProcessor,
  VideoProcessorConstructor,
} from './ffmpeg/video_processor.js';
import {
  createGifArgs,
  createMp4Args,
  createTimeLapseArgs,
} from './ffmpeg/video_processor_args.js';
import {createPrivateTempVideoFile} from './file_system.js';
import {FileAccessEntry} from './file_system_access_entry.js';

// This is used like a class constructor.
// eslint-disable-next-line @typescript-eslint/naming-convention
const FFMpegVideoProcessor = (async () => {
  const workerChannel = new MessageChannel();
  const videoProcessorHelper = await getVideoProcessorHelper();
  await videoProcessorHelper.connectToWorker(
      Comlink.transfer(workerChannel.port2, [workerChannel.port2]));
  return Comlink.wrap<VideoProcessorConstructor>(workerChannel.port1);
})();


/**
 * Creates a VideoProcessor instance for recording video.
 */
async function createVideoProcessor(output: AsyncWriter, videoRotation: number):
    Promise<Comlink.Remote<VideoProcessor>> {
  return new (await FFMpegVideoProcessor)(
      Comlink.proxy(output), createMp4Args(videoRotation, output.seekable()));
}

/**
 * Creates a VideoProcessor instance for recording gif.
 */
async function createGifVideoProcessor(
    output: AsyncWriter,
    resolution: Resolution): Promise<Comlink.Remote<VideoProcessor>> {
  return new (await FFMpegVideoProcessor)(
      Comlink.proxy(output), createGifArgs(resolution));
}

/*
 * Creates a VideoProcessor instance for recording time-lapse.
 */
async function createTimeLapseProcessor(
    output: AsyncWriter,
    resolution: Resolution): Promise<Comlink.Remote<VideoProcessor>> {
  return new (await FFMpegVideoProcessor)(
      Comlink.proxy(output), createTimeLapseArgs(resolution));
}

/**
 * Creates an AsyncWriter that writes to the given intent.
 */
function createWriterForIntent(intent: Intent): AsyncWriter {
  async function write(blob: Blob) {
    await intent.appendData(new Uint8Array(await blob.arrayBuffer()));
  }
  // TODO(crbug.com/1140852): Supports seek.
  return new AsyncWriter({write, seek: null, close: null});
}

/**
 * Used to save captured video.
 */
export class VideoSaver {
  constructor(
      private readonly file: FileAccessEntry,
      private readonly processor: Comlink.Remote<VideoProcessor>) {}

  /**
   * Writes video data to result video.
   */
  async write(blob: Blob): Promise<void> {
    await this.processor.write(blob);
  }

  /**
   * Cancels and drops all the written video data.
   */
  async cancel(): Promise<void> {
    await this.processor.cancel();
    return this.file.remove();
  }

  /**
   * Finishes the write of video data parts and returns result video file.
   *
   * @return Result video file.
   */
  async endWrite(): Promise<FileAccessEntry> {
    await this.processor.close();
    return this.file;
  }

  /**
   * Creates video saver which saves video into a temporary file.
   * TODO(b/184583382): Saves to the target file directly once the File System
   * Access API supports cleaning temporary file when leaving the page without
   * closing the file stream.
   */
  static async create(videoRotation: number): Promise<VideoSaver> {
    const file = await createPrivateTempVideoFile();
    const writer = await file.getWriter();
    const processor = await createVideoProcessor(writer, videoRotation);
    return new VideoSaver(file, processor);
  }

  /**
   * Creates video saver for the given intent.
   */
  static async createForIntent(intent: Intent, videoRotation: number):
      Promise<VideoSaver> {
    const file = await createPrivateTempVideoFile();
    const fileWriter = await file.getWriter();
    const intentWriter = createWriterForIntent(intent);
    const writer = AsyncWriter.combine(fileWriter, intentWriter);
    const processor = await createVideoProcessor(writer, videoRotation);
    return new VideoSaver(file, processor);
  }
}

/**
 * Used to save captured gif.
 */
export class GifSaver {
  constructor(
      private readonly blobs: Blob[],
      private readonly processor: Comlink.Remote<VideoProcessor>) {}

  async write(frame: Uint8ClampedArray): Promise<void> {
    await this.processor.write(new Blob([frame]));
  }

  /**
   * Finishes the write of gif data parts and returns result gif blob.
   */
  async endWrite(): Promise<Blob> {
    await this.processor.close();
    return new Blob(this.blobs, {type: MimeType.GIF});
  }

  /**
   * Creates video saver for the given file.
   */
  static async create(resolution: Resolution): Promise<GifSaver> {
    const blobs: Blob[] = [];
    const writer = new AsyncWriter({
      write(blob) {
        blobs.push(blob);
      },
      seek: null,
      close: null,
    });
    const processor = await createGifVideoProcessor(writer, resolution);
    return new GifSaver(blobs, processor);
  }
}

interface FrameInfo {
  chunk: Blob;
  frameNo: number;
}

/**
 * Used to save time-lapse video.
 */
export class TimeLapseSaver {
  /**
   * Video encoder used to encode frame.
   */
  private readonly encoder: VideoEncoder;

  /**
   * Map a frame's timestamp with frameNo, only store frame that's being
   * encoded.
   */
  private readonly frameNoMap = new Map<number, number>();

  /**
   * Store all encoded frames with its frame numbers.
   * TODO(b/236800499): Investigate if it is OK to store number of blobs in
   * memory.
   */
  private readonly frames: FrameInfo[] = [];

  private speed: number;

  constructor(
      encoderConfig: VideoEncoderConfig,
      private readonly resolution: Resolution, initialSpeed: number) {
    this.speed = initialSpeed;
    this.encoder = new VideoEncoder({
      error: (error) => {
        throw error;
      },
      output: (chunk) => this.onFrameEncoded(chunk),
    });
    this.encoder.configure(encoderConfig);
  }

  onFrameEncoded(chunk: EncodedVideoChunk): void {
    const frameNo = this.frameNoMap.get(chunk.timestamp);
    assert(frameNo !== undefined);
    const chunkData = new Uint8Array(chunk.byteLength);
    chunk.copyTo(chunkData);
    this.frames.push({
      chunk: new Blob([chunkData]),
      frameNo,
    });
    this.frameNoMap.delete(chunk.timestamp);
  }

  write(frame: VideoFrame, frameNo: number): void {
    if (!frame.timestamp) {
      return;
    }
    this.frameNoMap.set(frame.timestamp, frameNo);
    this.encoder.encode(frame, {keyFrame: true});
  }

  updateSpeed(newSpeed: number): void {
    this.speed = newSpeed;
  }

  getSpeed(): number {
    return this.speed;
  }

  /**
   * Finishes the write of video data parts and returns result video file.
   *
   * @return Result video file.
   */
  async endWrite(): Promise<FileAccessEntry> {
    // TODO(b/236800499): Optimize file writing mechanism to make it faster.
    const file = await createPrivateTempVideoFile();
    const writer = await file.getWriter();
    const processor = await createTimeLapseProcessor(writer, this.resolution);

    const filteredChunk =
        this.frames.filter(({frameNo}) => frameNo % this.speed === 0);
    for (const {chunk} of filteredChunk) {
      processor.write(chunk);
    }
    await processor.close();
    this.encoder.close();

    return file;
  }

  /**
   * Creates video saver with encoder using provided |encoderConfig|.
   */
  static async create(
      encoderConfig: VideoEncoderConfig, resolution: Resolution,
      initialSpeed: number): Promise<TimeLapseSaver> {
    const encoderSupport = await VideoEncoder.isConfigSupported(encoderConfig);
    if (!encoderSupport.supported) {
      throw new Error('Video encoder is not supported.');
    }

    return new TimeLapseSaver(encoderConfig, resolution, initialSpeed);
  }
}
