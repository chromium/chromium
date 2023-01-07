// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Comlink from '../lib/comlink.js';

/**
 * A barcode worker to detect barcode from images.
 */
class BarcodeWorkerImpl {
  private readonly detector = new BarcodeDetector({formats: ['qr_code']});

  async detect(bitmap: ImageBitmap): Promise<string|null> {
    const codes = await this.detector.detect(bitmap);

    const cx = bitmap.width / 2;
    const cy = bitmap.height / 2;
    function distanceToCenter(code: DetectedBarcode): number {
      const {left, right, top, bottom} = code.boundingBox;
      const x = (left + right) / 2;
      const y = (top + bottom) / 2;
      return Math.hypot(x - cx, y - cy);
    }

    let minDistance = Infinity;
    let bestCode: DetectedBarcode|null = null;
    for (const code of codes) {
      const distance = distanceToCenter(code);
      if (distance < minDistance) {
        bestCode = code;
        minDistance = distance;
      }
    }
    return bestCode === null ? null : bestCode.rawValue;
  }
}

// Only export types to ensure that the file is not imported by other files at
// runtime.
export type BarcodeWorker = BarcodeWorkerImpl;

Comlink.expose(new BarcodeWorkerImpl());
