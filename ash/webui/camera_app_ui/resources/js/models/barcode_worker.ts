// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as comlink from '../lib/comlink.js';

/**
 * A barcode worker to detect barcode from images.
 */
class BarcodeWorkerImpl {
  // BarcodeDetector should always be available on ChromeOS. The check is used
  // for local development server.
  private readonly detector = 'BarcodeDetector' in self ?
      new BarcodeDetector({formats: ['qr_code']}) :
      null;

  async detect(bitmap: ImageBitmap): Promise<DetectedBarcode[]> {
    if (this.detector === null) {
      return [];
    }
    try {
      // TODO(chuhsuan): Check why this cannot be returned directly
      // (@typescript-eslint/return-await).
      const codes = await this.detector.detect(bitmap);
      return codes;
    } catch {
      // Barcode detection service unavailable.
      return [];
    }
  }
}

// Only export types to ensure that the file is not imported by other files at
// runtime.
export type BarcodeWorker = BarcodeWorkerImpl;

comlink.expose(new BarcodeWorkerImpl());
