// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists} from './assert.js';
import {Flag} from './flag.js';
import {OcrEventType, sendOcrEvent} from './metrics.js';
import {AsyncIntervalRunner} from './models/async_interval.js';
import {BarcodeScanner, ScanBarcodeResult} from './models/barcode.js';
import {getChromeFlag} from './models/load_time_data.js';
import {Ocr, PerformOcrResult} from './ocr.js';
import {PerfLogger} from './perf.js';
import * as scannerChip from './scanner_chip.js';
import * as state from './state.js';
import {OneShotTimer} from './timer.js';
import {PerfEvent} from './type.js';

// The interval between consecutive preview scans in milliseconds.
export const BARCODE_SCAN_INTERVAL = 200;
export const OCR_SCAN_INTERVAL = 500;

// The delay time to keep `SCAN_INTERVAL` in milliseconds. After this delay, the
// interval becomes `SCAN_INTERVAL_SLOW`.
export const SLOWDOWN_DELAY = 3 * 60 * 1000;

// The interval after `SLOWDOWN_DELAY` of idle in milliseconds.
export const BARCODE_SCAN_INTERVAL_SLOW = 1000;
export const OCR_SCAN_INTERVAL_SLOW = 1000;

type OcrResultLine = PerformOcrResult['result']['lines'][number];

interface DetectedResult {
  // Returns the minimal distance between the center of the detected content and
  // the center of the image. The distance should be normalized by the
  // dimensions of the source image, meaning it's a value between 0 (center) and
  // `Math.hypot(0.5, 0.5)` (corner).
  getNormalizedDistanceToCenter(): number;
  // Shows the detected content on the preview.
  show(): void;
}

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

const INITIAL_CLOSEST_CONTENT = {
  source: null,
  distance: Infinity,
};

export class PhotoModeAutoScanner {
  private slowdownTimer: OneShotTimer|null = null;

  private barcodeRunner: AsyncIntervalRunner|null = null;

  private ocrRunner: AsyncIntervalRunner|null = null;

  // `closestContent` tracks the detected content that is closest to the center
  // of the preview, since we have multiple scanners running simultaneously. See
  // `handleDetectedResult()` for more details.
  private closestContent: {
    source: scannerChip.Source|null,
    distance: number,
  } = INITIAL_CLOSEST_CONTENT;

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
    this.barcodeRunner = this.createBarcodeRunner(BARCODE_SCAN_INTERVAL);
    if (getChromeFlag(Flag.PREVIEW_OCR)) {
      this.ocrRunner = this.createOcrRunner(OCR_SCAN_INTERVAL);
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
    scannerChip.dismiss();
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
      this.barcodeRunner = this.createBarcodeRunner(BARCODE_SCAN_INTERVAL_SLOW);
    }
    if (this.ocrRunner !== null) {
      this.ocrRunner.stop();
      this.ocrRunner = this.createOcrRunner(OCR_SCAN_INTERVAL_SLOW);
    }
  }

  private createBarcodeRunner(interval: number) {
    const barcodeScanner = new BarcodeScanner(
        this.video,
        () => {
            // We don't use the BarcodeScanner.start so callback does nothing.
            // TODO(pihsun): callback should be directly passed to `start`
            // instead of in constructor.
        });
    return new AsyncIntervalRunner(async (stopped) => {
      const result = await barcodeScanner.scan();
      if (stopped.isSignaled()) {
        return;
      }
      this.handleDetectedResult(
          processBarcodeResult(result), scannerChip.Source.BARCODE);
    }, interval);
  }

  private createOcrRunner(interval: number) {
    const ocrScanner = new Ocr(this.video);
    const perfLogger = PerfLogger.getInstance();
    return new AsyncIntervalRunner(async (stopped) => {
      if (!state.get(state.State.ENABLE_PREVIEW_OCR)) {
        return;
      }
      const startTime = performance.now();
      const result = await ocrScanner.performOcr();
      if (stopped.isSignaled() || !state.get(state.State.ENABLE_PREVIEW_OCR)) {
        return;
      }
      // Use `startTime` here because if `performOcr()` takes too long, another
      // `performOcr()` might have started before the previous call finished.
      // For example, taking photo will stop the current OCR runner and create
      // a new one.
      perfLogger.start(PerfEvent.OCR_SCANNING, startTime);
      // TODO(chuhsuan): Add other dimensions like `facing` and `resolution`.
      perfLogger.stop(PerfEvent.OCR_SCANNING);
      this.ocrScanCount += 1;
      this.ocrScanTime += performance.now() - startTime;
      this.handleDetectedResult(
          processOcrResult(result), scannerChip.Source.OCR);
    }, interval);
  }

  /**
   * Checks detected results from scanners and decides whether to show them.
   *
   * When a scanner detects nothing, meaning there is a `source` but not a
   * `distance`, if `source` matches `closestContent.source`, reset
   * `closestContent` to its initial state, allowing other scanners to beat the
   * `distance`.
   *
   * When a scanner detects content, meaning there is a `source` and a finite
   * `distance`, show the content and set the value of `closestContent` if one
   * of the following conditions is met:
   *   - `closestContent` is in its initial state.
   *   - `source` matches `closestContent.source`.
   *   - `distance` is smaller than `closestContent.distance`.
   */
  private handleDetectedResult(
      detectedResult: DetectedResult|null, source: scannerChip.Source) {
    if (detectedResult === null) {
      if (source === this.closestContent.source) {
        this.closestContent = INITIAL_CLOSEST_CONTENT;
      }
      return;
    }

    let distance = Infinity;
    // TODO(b/341616441): Remove this once we find a way to prevent OCR from
    // detecting content in barcodes. Set `distance` to `Infinity` for OCR to
    // make it always larger than the `distance`s from barcode scanner.
    if (source !== scannerChip.Source.OCR) {
      distance = detectedResult.getNormalizedDistanceToCenter();
    }

    if (this.closestContent === INITIAL_CLOSEST_CONTENT ||
        source === this.closestContent.source ||
        distance < this.closestContent.distance) {
      this.closestContent = {
        source,
        distance,
      };
      detectedResult.show();
    }
  }
}

function processBarcodeResult(scanBarcodeResult: ScanBarcodeResult|
                              null): DetectedResult|null {
  if (scanBarcodeResult === null) {
    return null;
  }
  const {barcode, imageWidth, imageHeight} = scanBarcodeResult;
  function getNormalizedDistanceToCenter() {
    const {top, right, bottom, left} = barcode.boundingBox;
    const x = (left + right) / 2;
    const y = (top + bottom) / 2;
    return Math.hypot(
        x / imageWidth - 0.5,
        y / imageHeight - 0.5,
    );
  }
  function show() {
    scannerChip.showBarcodeContent(barcode.rawValue);
  }
  return {
    getNormalizedDistanceToCenter,
    show,
  };
}

function processOcrResult(performOcrResult: PerformOcrResult): DetectedResult|
    null {
  const {result, imageWidth, imageHeight} = performOcrResult;
  const lines = result.lines.filter((line) => line.confidence >= 0.9);

  // Calculates the minimum normalized distance to the center of the image from
  // all detected lines.
  let minNormalizedDistanceToCenter = Infinity;
  for (const line of lines) {
    const {x, y} = getCenterOfLine(line);
    const distance = Math.hypot(
        x / imageWidth - 0.5,
        y / imageHeight - 0.5,
    );
    if (distance < minNormalizedDistanceToCenter) {
      minNormalizedDistanceToCenter = distance;
    }
  }

  // Filter out the OCR result if no lines are close enough to the center of the
  // image.
  const maxPossibleDistance = Math.hypot(0.5, 0.5);
  if (minNormalizedDistanceToCenter > maxPossibleDistance / 2) {
    return null;
  }

  // Calculates the center point of the bounding box of the line. The origin of
  // the coordinate system is at the top-left corner. The bounding box is
  // rotated by `boundingBoxAngle` degrees in a clockwise direction.
  function getCenterOfLine(line: OcrResultLine) {
    const {x, y, width, height} = line.boundingBox;
    const lineTheta = line.boundingBoxAngle / 180 * Math.PI;
    const diagonalLength = Math.hypot(width, height) / 2;
    const diagonalTheta = Math.atan(height / width) + lineTheta;
    return {
      x: x + diagonalLength * Math.cos(diagonalTheta),
      y: y + diagonalLength * Math.sin(diagonalTheta),
    };
  }

  const filteredResult = {...result, lines};
  function show() {
    sendOcrEvent({
      eventType: OcrEventType.TEXT_DETECTED,
      result: filteredResult,
    });
    scannerChip.showOcrContent(filteredResult);
  }
  return {
    getNormalizedDistanceToCenter: () => minNormalizedDistanceToCenter,
    show,
  };
}
