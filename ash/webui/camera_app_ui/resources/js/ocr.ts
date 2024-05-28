// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from './assert.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {OcrResultLine} from './mojo/type.js';
import {ScanResult} from './photo_mode_auto_scanner.js';

export class Ocr {
  constructor(private readonly video: HTMLVideoElement) {}

  async performOcr(): Promise<ScanResult|null> {
    const width = this.video.videoWidth;
    const height = this.video.videoHeight;
    // TODO(b/342315479): Unify the way to capture image from preview.
    const canvas = new OffscreenCanvas(width, height);
    const ctx = assertInstanceof(
        canvas.getContext('2d'), OffscreenCanvasRenderingContext2D);
    ctx.drawImage(this.video, 0, 0);
    const blob = await canvas.convertToBlob({type: 'image/jpeg'});
    const result = await ChromeHelper.getInstance().performOcr(blob);
    if (result.lines.length === 0) {
      return null;
    }
    const value = result.lines.map((line) => line.text).join('\n');
    const distance =
        getMinNormalizedDistanceToCenter(result.lines, width, height);
    return {
      value,
      distance,
    };
  }
}

function getMinNormalizedDistanceToCenter(
    lines: OcrResultLine[], imageWidth: number, imageHeight: number) {
  let minDistance = Infinity;
  for (const line of lines) {
    const {x, y} = getCenterOfLine(line);
    const distance = Math.hypot(
        x / imageWidth - 0.5,
        y / imageHeight - 0.5,
    );
    if (distance < minDistance) {
      minDistance = distance;
    }
  }
  return minDistance;
}

// Calculates the center point of the bounding box of the line. The origin of
// the coordinate system is at the top-left corner. The bounding box is rotated
// by `boundingBoxAngle` degrees in a clockwise direction.
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
