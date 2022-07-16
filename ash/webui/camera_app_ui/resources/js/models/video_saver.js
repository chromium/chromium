// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Intent} from '../intent.js';  // eslint-disable-line no-unused-vars
import * as Comlink from '../lib/comlink.js';
import {
  MimeType,
  Resolution,  // eslint-disable-line no-unused-vars
} from '../type.js';
// eslint-disable-next-line no-unused-vars
import {VideoProcessorHelperInterface} from '../untrusted_helper_interfaces.js';
import * as util from '../util.js';

import {AsyncWriter} from './async_writer.js';
import {
  createGifArgs,
  createMp4Args,
} from './ffmpeg/video_processor_args.js';
import {createPrivateTempVideoFile} from './file_system.js';
// eslint-disable-next-line no-unused-vars
import {FileAccessEntry} from './file_system_access_entry.js';
// eslint-disable-next-line no-unused-vars
import {VideoProcessor} from './video_processor_interface.js';

const FFMpegVideoProcessor = (async () => {
  const workerChannel = new MessageChannel();
  const videoProcessorHelper = /** @type {!VideoProcessorHelperInterface} */ (
      await util.createUntrustedJSModule(
          '/js/untrusted_video_processor_helper.js'));
  await videoProcessorHelper.connectToWorker(
      Comlink.transfer(workerChannel.port2, [workerChannel.port2]));
  return Comlink.wrap(workerChannel.port1);
})();

/**
 * @param {!AsyncWriter} output
 * @param {number} videoRotation
 * @return {!Promise<!VideoProcessor>}
 */
async function createVideoProcessor(output, videoRotation) {
  // Comlink proxies all calls asynchronously, including constructors.
  return new (await FFMpegVideoProcessor)(
      Comlink.proxy(output), createMp4Args(videoRotation, output.seekable()));
}

/**
 * @param {!AsyncWriter} output
 * @param {!Resolution} resolution
 * @return {!Promise<!VideoProcessor>}
 */
async function createGifVideoProcessor(output, resolution) {
  return new (await FFMpegVideoProcessor)(
      Comlink.proxy(output), createGifArgs(resolution));
}

/**
 * @param {!Intent} intent
 * @return {!AsyncWriter}
 */
function createWriterForIntent(intent) {
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
  /**
   * @param {!FileAccessEntry} file
   * @param {!VideoProcessor} processor
   */
  constructor(file, processor) {
    /**
     * @const {!FileAccessEntry}
     * @private
     */
    this.file_ = file;

    /**
     * @const {!VideoProcessor}
     * @private
     */
    this.processor_ = processor;
  }

  /**
   * Writes video data to result video.
   * @param {!Blob} blob
   */
  write(blob) {
    this.processor_.write(blob);
  }

  /**
   * Cancels and drops all the written video data.
   * @return {!Promise}
   */
  async cancel() {
    await this.processor_.cancel();
    return this.file_.delete();
  }

  /**
   * Finishes the write of video data parts and returns result video file.
   * @return {!Promise<!FileAccessEntry>} Result video file.
   */
  async endWrite() {
    await this.processor_.close();
    return this.file_;
  }

  /**
   * Creates video saver for the given file.
   * @param {!FileAccessEntry} file
   * @param {number} videoRotation
   * @return {!Promise<!VideoSaver>}
   */
  static async createForFile(file, videoRotation) {
    const writer = await file.getWriter();
    const processor = await createVideoProcessor(writer, videoRotation);
    return new VideoSaver(file, processor);
  }

  /**
   * Creates video saver for the given intent.
   * @param {!Intent} intent
   * @param {number} videoRotation
   * @return {!Promise<!VideoSaver>}
   */
  static async createForIntent(intent, videoRotation) {
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
  /**
   * @param {!Array<!Blob>} blobs
   * @param {!VideoProcessor} processor
   */
  constructor(blobs, processor) {
    /**
     * @const {!Array<!Blob>}
     * @private
     */
    this.blobs_ = blobs;

    /**
     * @const {!VideoProcessor}
     * @private
     */
    this.processor_ = processor;
  }

  /**
   * @param {!Uint8ClampedArray} frame
   */
  write(frame) {
    this.processor_.write(new Blob([frame]));
  }

  /**
   * Finishes the write of gif data parts and returns result gif blob.
   * @return {!Promise<!Blob>}
   */
  async endWrite() {
    await this.processor_.close();
    return new Blob(this.blobs_, {type: MimeType.GIF});
  }

  /**
   * Creates video saver for the given file.
   * @param {!Resolution} resolution
   * @return {!Promise<!GifSaver>}
   */
  static async create(resolution) {
    const blobs = [];
    const writer = new AsyncWriter({
      write: async (blob) => blobs.push(blob),
      seek: null,
      close: null,
    });
    const processor = await createGifVideoProcessor(writer, resolution);
    return new GifSaver(blobs, processor);
  }
}
