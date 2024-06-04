// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from './assert.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {OcrResult} from './mojo/type.js';

export interface PerformOcrResult {
  result: OcrResult;
  imageWidth: number;
  imageHeight: number;
}

export class Ocr {
  constructor(private readonly video: HTMLVideoElement) {}

  async performOcr(): Promise<PerformOcrResult> {
    const width = this.video.videoWidth;
    const height = this.video.videoHeight;
    // TODO(b/342315479): Unify the way to capture image from preview.
    const canvas = new OffscreenCanvas(width, height);
    const ctx = assertInstanceof(
        canvas.getContext('2d'), OffscreenCanvasRenderingContext2D);
    ctx.drawImage(this.video, 0, 0);
    const blob = await canvas.convertToBlob({type: 'image/jpeg'});
    const result = await ChromeHelper.getInstance().performOcr(blob);
    return {
      result,
      imageWidth: width,
      imageHeight: height,
    };
  }
}
