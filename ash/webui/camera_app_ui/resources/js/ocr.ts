// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from './assert.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {OcrResult} from './mojo/type.js';

export class Ocr {
  constructor(private readonly video: HTMLVideoElement) {}

  async performOcr(): Promise<OcrResult> {
    const canvas =
        new OffscreenCanvas(this.video.videoWidth, this.video.videoHeight);
    const ctx = assertInstanceof(
        canvas.getContext('2d'), OffscreenCanvasRenderingContext2D);
    ctx.drawImage(this.video, 0, 0);
    const blob = await canvas.convertToBlob({type: 'image/jpeg'});
    const result = await ChromeHelper.getInstance().performOcr(blob);
    return result;
  }
}
