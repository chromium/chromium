// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists} from './assert.js';
import * as barcodeChip from './barcode_chip.js';
import {Flag} from './flag.js';
import {AsyncIntervalRunner} from './models/async_interval.js';
import {BarcodeScanner} from './models/barcode.js';
import {getChromeFlag} from './models/load_time_data.js';
import {Ocr} from './ocr.js';
import {OneShotTimer} from './timer.js';

// The delay interval between consecutive preview scans in milliseconds.
export const SCAN_INTERVAL = 200;

// The delay time to keep `SCAN_INTERVAL` in milliseconds. After this delay, the
// interval becomes `SCAN_INTERVAL_SLOW`.
export const SLOWDOWN_DELAY = 3 * 60 * 1000;

// The delay interval after `SLOWDOWN_DELAY` of idle in milliseconds.
export const SCAN_INTERVAL_SLOW = 1000;

// The last created `PhotoModeAutoScanner` instance.
let instance: PhotoModeAutoScanner|null = null;

/**
 * Creates a `PhotoModeAutoScanner` instance.
 */
export function createInstance(video: HTMLVideoElement): PhotoModeAutoScanner {
  instance = new PhotoModeAutoScanner(video);
  return instance;
}

/**
 * Get the `PhotoModeAutoScanner` instance for testing purpose.
 */
export function getInstanceForTest(): PhotoModeAutoScanner {
  return assertExists(instance);
}

export class PhotoModeAutoScanner {
  private slowdownTimer: OneShotTimer|null = null;

  private barcodeRunner: AsyncIntervalRunner|null = null;

  private ocrRunner: AsyncIntervalRunner|null = null;

  /**
   * The number of OCR scans, only used in tests. Reset when calling `stop()`.
   */
  private ocrScanCount = 0;

  /**
   * The accumulated time of OCR scans, only used in tests. Reset when calling
   * `stop()`.
   */
  private ocrScanTime = 0;

  constructor(private readonly video: HTMLVideoElement) {}

  start(): void {
    // TODO(b/311592341): Show the object closer to the center of preview when
    // both scanners detect objects at the same time.
    if (getChromeFlag(Flag.AUTO_QR)) {
      this.barcodeRunner = this.createBarcodeRunner(SCAN_INTERVAL);
    }
    if (getChromeFlag(Flag.PREVIEW_OCR)) {
      this.ocrRunner = this.createOcrRunner(SCAN_INTERVAL);
    }
    this.slowdownTimer = new OneShotTimer(() => {
      this.slowdownTimer = null;
      this.slowdown();
    }, SLOWDOWN_DELAY);
  }

  restart(): void {
    this.stop();
    this.start();
  }

  stop(): void {
    barcodeChip.dismiss();
    this.slowdownTimer?.stop();
    this.slowdownTimer = null;
    this.barcodeRunner?.stop();
    this.barcodeRunner = null;
    this.ocrRunner?.stop();
    this.ocrRunner = null;
    this.ocrScanCount = 0;
    this.ocrScanTime = 0;
  }

  getAverageOcrScanTime(): number {
    return this.ocrScanTime / this.ocrScanCount;
  }

  private slowdown() {
    if (this.barcodeRunner !== null) {
      this.barcodeRunner.stop();
      this.barcodeRunner = this.createBarcodeRunner(SCAN_INTERVAL_SLOW);
    }
    if (this.ocrRunner !== null) {
      this.ocrRunner.stop();
      this.ocrRunner = this.createOcrRunner(SCAN_INTERVAL_SLOW);
    }
  }

  private createBarcodeRunner(interval: number) {
    const barcodeScanner = new BarcodeScanner(this.video, () => {});
    return new AsyncIntervalRunner(async (stopped) => {
      const result = await barcodeScanner.scan();
      if (stopped.isSignaled() || result === null) {
        return;
      }
      barcodeChip.show(result);
    }, interval);
  }

  private createOcrRunner(interval: number) {
    const ocrScanner = new Ocr(this.video);
    return new AsyncIntervalRunner(async (stopped) => {
      const startTime = performance.now();
      const result = await ocrScanner.performOcr();
      if (stopped.isSignaled()) {
        return;
      }
      this.ocrScanCount += 1;
      this.ocrScanTime += performance.now() - startTime;
      if (result.lines.length === 0) {
        return;
      }
      const text = result.lines.map((line) => line.text).join('\n');
      barcodeChip.showOcrText(text);
    }, interval);
  }
}
