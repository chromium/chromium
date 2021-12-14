// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Intent} from '../intent.js';
import * as Comlink from '../lib/comlink.js';
import {
  MimeType,
  Resolution,
} from '../type.js';
import {VideoProcessorHelperInterface} from '../untrusted_helper_interfaces.js';
import * as util from '../util.js';

import {AsyncWriter} from './async_writer.js';
import {
  createGifArgs,
  createMp4Args,
} from './ffmpeg/video_processor_args.js';
import {createPrivateTempVideoFile} from './file_system.js';
import {FileAccessEntry} from './file_system_access_entry.js';
import {VideoProcessor} from './video_processor_interface.js';

const FFMpegVideoProcessor = (async () => {
  const workerChannel = new MessageChannel();
  const videoProcessorHelper =
      await util.createUntrustedJSModule<VideoProcessorHelperInterface>(
          '/js/untrusted_video_processor_helper.js');
  await videoProcessorHelper.connectToWorker(
      Comlink.transfer(workerChannel.port2, [workerChannel.port2]));
  return Comlink.wrap<typeof VideoProcessor>(workerChannel.port1);
})();


async function createVideoProcessor(
    output: AsyncWriter, videoRotation: number): Promise<VideoProcessor> {
  return new (await FFMpegVideoProcessor)(
      Comlink.proxy(output), createMp4Args(videoRotation, output.seekable()));
}

async function createGifVideoProcessor(
    output: AsyncWriter, resolution: Resolution): Promise<VideoProcessor> {
  return new (await FFMpegVideoProcessor)(
      Comlink.proxy(output), createGifArgs(resolution));
}

function createWriterForIntent(intent: Intent): AsyncWriter {
  const write = async (blob) => {
    await intent.appendData(new Uint8Array(await blob.arrayBuffer()));
  };
  // TODO(crbug.com/1140852): Supports seek.
  return new AsyncWriter({write, seek: null, close: null});
}

/**
 * Used to save captured video.
 */
export class VideoSaver {
  constructor(
      private readonly file: FileAccessEntry,
      private readonly processor: VideoProcessor) {}

  /**
   * Writes video data to result video.
   */
  write(blob: Blob): void {
    this.processor.write(blob);
  }

  /**
   * Cancels and drops all the written video data.
   */
  async cancel(): Promise<void> {
    await this.processor.cancel();
    return this.file.delete();
  }

  /**
   * Finishes the write of video data parts and returns result video file.
   * @return Result video file.
   */
  async endWrite(): Promise<FileAccessEntry> {
    await this.processor.close();
    return this.file;
  }

  /**
   * Creates video saver for the given file.
   */
  static async createForFile(file: FileAccessEntry, videoRotation: number):
      Promise<VideoSaver> {
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
      private readonly processor: VideoProcessor) {}

  write(frame: Uint8ClampedArray): void {
    this.processor.write(new Blob([frame]));
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
    const blobs = [];
    const writer = new AsyncWriter({
      async write(blob) {
        blobs.push(blob);
      },
      seek: null,
      close: null,
    });
    const processor = await createGifVideoProcessor(writer, resolution);
    return new GifSaver(blobs, processor);
  }
}
