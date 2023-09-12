// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof, assertNotReached} from './assert.js';
import {queuedAsyncCallback} from './async_job_queue.js';
import * as dom from './dom.js';
import {DeviceOperator} from './mojo/device_operator.js';
import {Resolution} from './type.js';

/**
 * Rotates the given coordinates in [0, 1] square space by the given
 * clockwise orientation.
 *
 * @return The rotated [x, y].
 */
function rotate(x: number, y: number, orientation: number): [number, number] {
  switch (orientation) {
    case 0:
      return [x, y];
    case 90:
      return [1 - y, x];
    case 180:
      return [1 - x, 1 - y];
    case 270:
      return [y, 1 - x];
    default:
      assertNotReached('Unexpected orientation');
  }
}

/**
 * An overlay to show face rectangles over preview.
 */
export class FaceOverlay {
  private readonly canvas = dom.get('#preview-face-overlay', HTMLCanvasElement);

  private readonly ctx: CanvasRenderingContext2D;

  private readonly orientationListener =
      queuedAsyncCallback('keepLatest', async () => {
        await this.updateOrientation();
      });

  /**
   * @param activeArraySize The active array size of the device.
   * @param orientation Clockwise angles to apply rotation to
   *     the face rectangles.
   */
  constructor(
      private readonly activeArraySize: Resolution, private orientation: number,
      private readonly deviceId: string) {
    this.ctx = assertInstanceof(
        this.canvas.getContext('2d'), CanvasRenderingContext2D);
    window.screen.orientation.addEventListener(
        'change', this.orientationListener);
  }

  /**
   * Updates orientation.
   */
  async updateOrientation(): Promise<void> {
    const deviceOperator = DeviceOperator.getInstance();
    if (deviceOperator !== null) {
      this.orientation =
          await deviceOperator.getCameraFrameRotation(this.deviceId);
    }
  }

  /**
   * Shows the given rectangles on overlay. The old rectangles would be
   * cleared, if any.
   *
   * @param rects An array of [x1, y1, x2, y2] to represent rectangles in the
   *     coordinate system of active array in sensor.
   */
  show(rects: number[]): void {
    this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

    // TODO(b/178344897): Handle zoomed preview.

    // TODO(pihsun): This currently doesn't change dynamically when the color
    // is changed, although the "warning" color is fixed in the current color
    // token design. It's still better to change drawing face overlay with SVG
    // instead of canvas for easier styling.
    const rectColor = getComputedStyle(document.documentElement)
                          .getPropertyValue('--cros-sys-warning');
    this.ctx.strokeStyle = rectColor;
    for (let i = 0; i < rects.length; i += 4) {
      let [x1, y1, x2, y2] = rects.slice(i, i + 4);
      x1 /= this.activeArraySize.width;
      y1 /= this.activeArraySize.height;
      x2 /= this.activeArraySize.width;
      y2 /= this.activeArraySize.height;
      [x1, y1] = rotate(x1, y1, this.orientation);
      [x2, y2] = rotate(x2, y2, this.orientation);

      const canvasAspectRatio = this.canvas.width / this.canvas.height;
      const sensorAspectRatio =
          this.orientation === 90 || this.orientation === 270 ?
          this.activeArraySize.height / this.activeArraySize.width :
          this.activeArraySize.width / this.activeArraySize.height;
      if (canvasAspectRatio > sensorAspectRatio) {
        // Canvas has wider aspect than the sensor, e.g. when we're showing a
        // 16:9 stream captured from a 4:3 sensor. Based on our hardware
        // requirement, we assume the stream is cropped into letterbox from the
        // active array.
        const normalizedCanvasHeight = sensorAspectRatio / canvasAspectRatio;
        const clipped = (1 - normalizedCanvasHeight) / 2;
        x1 *= this.canvas.width;
        y1 = (Math.max(y1 - clipped, 0) / normalizedCanvasHeight) *
            this.canvas.height;
        x2 *= this.canvas.width;
        y2 = (Math.max(y2 - clipped, 0) / normalizedCanvasHeight) *
            this.canvas.height;
      } else {
        // Canvas has taller aspect than the sensor, e.g. when we're showing a
        // 4:3 stream captured from a 16:9 sensor. Based on our hardware
        // requirement, we assume the stream is cropped into pillarbox from the
        // active array.
        const normalizedCanvasWidth = canvasAspectRatio / sensorAspectRatio;
        const clipped = (1 - normalizedCanvasWidth) / 2;
        x1 = (Math.max(x1 - clipped, 0) / normalizedCanvasWidth) *
            this.canvas.width;
        y1 *= this.canvas.height;
        x2 = (Math.max(x2 - clipped, 0) / normalizedCanvasWidth) *
            this.canvas.width;
        y2 *= this.canvas.height;
      }
      this.ctx.strokeRect(x1, y1, x2 - x1, y2 - y1);
    }
  }

  /**
   * Clears all rectangles.
   */
  clearRects(): void {
    this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
  }

  /**
   * Removes updateOrientation from the event listener and clears all
   * rectangles.
   */
  clear(): void {
    this.clearRects();
    window.screen.orientation.removeEventListener(
        'change', this.orientationListener);
  }
}
