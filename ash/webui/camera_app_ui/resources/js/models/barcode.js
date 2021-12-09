// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '../assert.js';
import * as Comlink from '../lib/comlink.js';

import {clearAsyncInterval, setAsyncInterval} from './async_interval.js';
// eslint-disable-next-line no-unused-vars
import {BarcodeWorkerInterface} from './barcode_worker_interface.js';

// The delay interval between consecutive barcode detections.
const SCAN_INTERVAL = 200;

// If any dimension of the video exceeds this size, the image would be cropped
// and/or scaled before scanning to speed up the detection.
const MAX_SCAN_SIZE = 720;

// The portion of the square in the middle that would be scanned for barcode.
// TODO(b/172879638): Change 1.0 to match the final UI spec.
const ACTIVE_SCAN_RATIO = 1.0;

/**
 * A barcode scanner to detect barcodes from a camera stream.
 */
export class BarcodeScanner {
  /**
   * @param {!HTMLVideoElement} video The video to be scanned for barcode.
   * @param {function(string): void} callback The callback for the detected
   *     barcodes.
   */
  constructor(video, callback) {
    /**
     * @type {!HTMLVideoElement}
     * @private
     */
    this.video_ = video;

    /**
     * @type {function(string): void}
     * @private
     */
    this.callback_ = callback;

    /**
     * @type {!BarcodeWorkerInterface}
     * @private
     */
    this.worker_ = Comlink.wrap(
        new Worker('/js/models/barcode_worker.js', {type: 'module'}));

    /**
     * The current running interval id.
     * @type {?number}
     */
    this.intervalId_ = null;
  }

  /**
   * Starts scanning barcodes continuously. Calling this method when it's
   * already started would be no-op.
   */
  start() {
    if (this.intervalId_ !== null) {
      return;
    }
    this.intervalId_ = setAsyncInterval(async () => {
      const code = await this.scan_();
      if (code !== null) {
        this.callback_(code);
      }
    }, SCAN_INTERVAL);
  }

  /**
   * Stops scanning barcodes.
   */
  stop() {
    if (this.intervalId_ === null) {
      return;
    }
    clearAsyncInterval(this.intervalId_);
    this.intervalId_ = null;
  }

  /**
   * Grabs the current video frame for scanning. If the video resolution is too
   * high, the image would be scaled and/or cropped from the center.
   * @return {!Promise<!ImageBitmap>}
   */
  async grabFrameForScan_() {
    const {videoWidth: vw, videoHeight: vh} = this.video_;
    if (vw <= MAX_SCAN_SIZE && vh <= MAX_SCAN_SIZE) {
      return createImageBitmap(this.video_);
    }

    const scanSize = Math.min(MAX_SCAN_SIZE, vw, vh);
    const ratio = ACTIVE_SCAN_RATIO * Math.min(vw / scanSize, vh / scanSize);
    const sw = ratio * scanSize;
    const sh = ratio * scanSize;
    const sx = (vw - sw) / 2;
    const sy = (vh - sh) / 2;

    // TODO(b/172879638): Figure out why drawing on canvas first is much faster
    // than createImageBitmap() directly.
    const canvas = new OffscreenCanvas(scanSize, scanSize);
    const ctx = assertInstanceof(
        canvas.getContext('2d', {alpha: false}),
        OffscreenCanvasRenderingContext2D);
    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';
    ctx.drawImage(this.video_, sx, sy, sw, sh, 0, 0, scanSize, scanSize);
    return canvas.transferToImageBitmap();
  }

  /**
   * Scans barcodes from the current frame.
   * @return {!Promise<?string>} The detected barcode value, or null if no
   *     barcode is detected.
   * @private
   */
  async scan_() {
    const frame = await this.grabFrameForScan_();
    const value = await this.worker_.detect(Comlink.transfer(frame, [frame]));
    return value;
  }
}
