// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '../assert.js';
import * as Comlink from '../lib/comlink.js';

import {AsyncIntervalRunner} from './async_interval.js';
import {BarcodeWorker} from './barcode_worker.js';

// The delay interval between consecutive barcode detections.
const SCAN_INTERVAL = 200;

// If any dimension of the video exceeds this size, the image would be cropped
// and/or scaled before scanning to speed up the detection.
const MAX_SCAN_SIZE = 720;

// The portion of the square in the middle that would be scanned for barcode.
// TODO(b/172879638): Change 1.0 to match the final UI spec.
const ACTIVE_SCAN_RATIO = 1.0;

const BARCODE_WORKER = Comlink.wrap<BarcodeWorker>(
    new Worker('/js/models/barcode_worker.js', {type: 'module'}));

/**
 * A barcode scanner to detect barcodes from a camera stream.
 */
export class BarcodeScanner {
  private scanRunner: AsyncIntervalRunner|null = null;

  /**
   * @param video The video to be scanned for barcode.
   * @param callback The callback for the detected barcodes.
   */
  constructor(
      private readonly video: HTMLVideoElement,
      private readonly callback: (barcode: string) => void) {}

  /**
   * Starts scanning barcodes continuously. Calling this method when it's
   * already started would be no-op.
   */
  start(): void {
    if (this.scanRunner !== null) {
      return;
    }
    this.scanRunner = new AsyncIntervalRunner(async (stopped) => {
      const code = await this.scan();
      if (!stopped.isSignaled() && code !== null) {
        this.callback(code);
      }
    }, SCAN_INTERVAL);
  }

  /**
   * Stops scanning barcodes.
   */
  stop(): void {
    if (this.scanRunner === null) {
      return;
    }
    this.scanRunner.stop();
    this.scanRunner = null;
  }

  /**
   * Grabs the current video frame for scanning. If the video resolution is too
   * high, the image would be scaled and/or cropped from the center.
   */
  private grabFrameForScan(): Promise<ImageBitmap> {
    const {videoWidth: vw, videoHeight: vh} = this.video;
    if (vw <= MAX_SCAN_SIZE && vh <= MAX_SCAN_SIZE) {
      return createImageBitmap(this.video);
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
    ctx.drawImage(this.video, sx, sy, sw, sh, 0, 0, scanSize, scanSize);
    return Promise.resolve(canvas.transferToImageBitmap());
  }

  /**
   * Scans barcodes from the current frame.
   *
   * @return The detected barcode value, or null if no barcode is detected.
   */
  private async scan(): Promise<string|null> {
    const frame = await this.grabFrameForScan();
    const value = await BARCODE_WORKER.detect(Comlink.transfer(frame, [frame]));
    return value;
  }
}
