// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from '../assert.js';
import * as comlink from '../lib/comlink.js';
import {BARCODE_SCAN_INTERVAL} from '../photo_mode_auto_scanner.js';
import * as state from '../state.js';
import {getSanitizedScriptUrl} from '../trusted_script_url_policy_util.js';
import {lazySingleton} from '../util.js';

import {AsyncIntervalRunner} from './async_interval.js';
import {BarcodeWorker} from './barcode_worker.js';

export interface ScanBarcodeResult {
  barcode: DetectedBarcode;
  imageWidth: number;
  imageHeight: number;
}

type BoundingBox = DetectedBarcode['boundingBox'];

// If any dimension of the video exceeds this size, the image would be cropped
// and/or scaled before scanning to speed up the detection.
const MAX_SCAN_SIZE = 720;

// The portion of the square in the middle that would be scanned for barcode.
// TODO(b/172879638): Change 1.0 to match the final UI spec.
const ACTIVE_SCAN_RATIO = 1.0;

const getBarcodeWorker = lazySingleton(
    () => comlink.wrap<BarcodeWorker>(new Worker(
        getSanitizedScriptUrl('/js/models/barcode_worker.js'),
        {type: 'module'})));

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
   *
   * @param scanIntervalMs Scan interval time. Unit is milliseconds.
   */
  start(scanIntervalMs = BARCODE_SCAN_INTERVAL): void {
    if (this.scanRunner !== null) {
      return;
    }
    this.scanRunner = new AsyncIntervalRunner(async (stopped) => {
      // Not show detected code during taking a photo
      if (state.get(state.State.TAKING)) {
        return;
      }

      const result = await this.scan();
      if (!stopped.isSignaled() && result !== null) {
        this.callback(result.barcode.rawValue);
      }
    }, scanIntervalMs);
  }

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
   * @return `ScanBarcodeResult` which contains the dimensions of the scanned
   * image and the barcode closest to the center. `null` if nothing is detected.
   */
  async scan(): Promise<ScanBarcodeResult|null> {
    const frame = await this.grabFrameForScan();
    const {width, height} = frame;
    const codes =
        await getBarcodeWorker().detect(comlink.transfer(frame, [frame]));
    if (codes.length === 0) {
      return null;
    }
    return {
      barcode: getBestBarcode(codes, width, height),
      imageWidth: width,
      imageHeight: height,
    };
  }
}

/**
 * Returns the barcode that is closest to the center of the scanned image.
 */
function getBestBarcode(
    barcodes: DetectedBarcode[], imageWidth: number,
    imageHeight: number): DetectedBarcode {
  assert(barcodes.length > 0);
  let minDistance = Infinity;
  let codeWithMinDistance = barcodes[0];
  for (const code of barcodes) {
    const distance =
        getDistanceToCenter(code.boundingBox, imageWidth, imageHeight);
    if (distance < minDistance) {
      minDistance = distance;
      codeWithMinDistance = code;
    }
  }
  return codeWithMinDistance;
}

function getDistanceToCenter(
    boundingBox: BoundingBox, imageWidth: number, imageHeight: number) {
  const {top, right, bottom, left} = boundingBox;
  const cx = imageWidth / 2;
  const cy = imageHeight / 2;
  const x = (left + right) / 2;
  const y = (top + bottom) / 2;
  const distance = Math.hypot(
      x - cx,
      y - cy,
  );
  return distance;
}
