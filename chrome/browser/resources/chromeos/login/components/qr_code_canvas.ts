// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

// Colors for drawing.
const BLACK_FILL: string = '#202124';
const WHITE_FILL: string = '#ffffff';

// Default size to use for the canvas on first draw, before the resize event.
const DEFAULT_SIZE: number = 400;

// When drawing circles, use this value to determine how big the radius of
// the small dots should be.
const RADIUS_PERCENTAGE: number = 0.75;

//  QrCodeCanvas
//  ----------------
//  Utility for drawing QR codes using circles or squares. Currently used by
//  QuickStart. The QR code input data contains a bit for each cell dictating
//  whether or not it should be drawn.
//
//     ___________ ...   Each of the square cells of the QR code will be drawn
//    |    |    |        using `cellSizePx` pixels. The circles are then
//    |____|____|_ ...   embedded into these squares with their centers matching
//    |    |    |        the square's center. The radius of the circles is
//    :    :    :        controlled by `RADIUS_PERCENTAGE`.
//
//   A ResizeObserver is attached to the given <canvas> and monitors any size
//   changes, triggering a redraw with optimal sizes based on the screen's DPI.

interface QrCanvasElementType extends HTMLCanvasElement {
  qrCellCount?: number;
}

export class QrCodeCanvas {
  private canvas: QrCanvasElementType;
  private context: CanvasRenderingContext2D;
  private observer: ResizeObserver;
  private qrCode: boolean[] | null;
  private useCircles: boolean;
  private canvasSize: number;
  private cellCount: number;
  private cellSizePx: number;
  private dotRadiusPx: number;
  private fiducialCellCount: number;

  constructor(canvas: HTMLCanvasElement) {
    this.canvas = canvas;
    const context = canvas.getContext('2d');
    assert(context instanceof CanvasRenderingContext2D);
    this.context = context;

    // Observe size changes.
    this.observer = new ResizeObserver(this.onResize.bind(this));
    this.observer.observe(this.canvas);

    // Default values
    this.qrCode = null;
    this.useCircles = true;
    this.canvasSize = 0;
    this.cellCount = 0;
    this.cellSizePx = 0;
    this.dotRadiusPx = 0;
    this.fiducialCellCount = 0;
  }

  setData(qrData: boolean[]): void {
    this.qrCode = qrData;
    this.updateInternal();
    this.drawQrCode();
  }

  private updateInternal(): void {
    // The true size of the canvas on screen right now.
    const canvasSizeOnScreen = Math.ceil(this.canvas.clientHeight);

    // Ideal size used for drawing, taking into account the screen's DPI.
    // If the canvas has no size yet, use a default value until resize occurs.
    const canvasSize = canvasSizeOnScreen > 0 ?
      canvasSizeOnScreen * window.devicePixelRatio * 2 :
      DEFAULT_SIZE;
    this.canvasSize = canvasSize;
    this.canvas.height = this.canvas.width = this.canvasSize;

    if (this.qrCode) {
      this.cellCount = Math.round(Math.sqrt(this.qrCode.length));
      this.cellSizePx = this.canvasSize / this.cellCount;
    } else {
      this.cellCount = 0;
      this.cellSizePx = 0;
    }
    this.dotRadiusPx = (this.cellSizePx / 2) * RADIUS_PERCENTAGE;
    this.fiducialCellCount = this.determineFiducialSize();
    if (this.fiducialCellCount === 0) {
      // Could not determine. Default to drawing squares.
      this.useCircles = false;
    }

    // Add cellCount to the <canvas> as an attribute for browsertests.
    this.canvas.qrCellCount = this.cellCount;
  }

  toggleMode(): void {
    this.useCircles = !this.useCircles;
    this.updateInternal();
    this.drawQrCode();
  }

  private drawQrCode(): void {
    // Draw all of the cells of the QR code.
    this.drawDataPoints();

    // Redraw the fiducial markers as circles if necessary.
    if (this.useCircles) {
      // Draw fiducials
      const fiducialSizePx = this.fiducialCellCount * this.cellSizePx;
      const fiducialRadiusPx = fiducialSizePx / 2;

      // Upper left.
      this.drawFiducial(
        /* xPos= */ 0 + fiducialRadiusPx,
        /* yPos= */ 0 + fiducialRadiusPx,
        fiducialSizePx);

      // Upper right
      this.drawFiducial(
        /* xPos= */ this.canvasSize - fiducialRadiusPx,
        /* yPos= */ 0 + fiducialRadiusPx,
        fiducialSizePx);

      // Lower left
      this.drawFiducial(
        /* xPos= */ 0 + fiducialRadiusPx,
        /* yPos= */ this.canvasSize - fiducialRadiusPx,
        fiducialSizePx);
    }

  }

  private drawDataPoints(): void {
    this.context.clearRect(0, 0, this.canvasSize, this.canvasSize);
    this.context.fillStyle = BLACK_FILL;

    // Offset the circles by half of the tile size in order to draw them
    // in the center of the tile. Without this, the circles at the upper
    // and left edges of the code would consist of half circles instead of
    // full circles.
    const OFFSET = this.cellSizePx / 2;

    for (let y = 0; y < this.cellCount; y++) {
      for (let x = 0; x < this.cellCount; x++) {
        const index = y * this.cellCount + x;
        if (this.qrCode && this.qrCode[index]) {
          if (this.useCircles) {
            this.context.beginPath();
            this.context.arc(
              x * this.cellSizePx + OFFSET,
              y * this.cellSizePx + OFFSET,
              this.dotRadiusPx, 0, 2 * Math.PI);
            this.context.fill();
          } else {
            this.context.fillRect(
              x * this.cellSizePx,
              y * this.cellSizePx,
              this.cellSizePx, this.cellSizePx);
          }
        }
      }
    }
  }

  // Draw a circular fiducial marker at the given location. It consists
  // of 3 concentric circles whereas each circle is smaller than the
  // previous one by the size of one cell.
  private drawFiducial(xPos: number, yPos: number, sizeInPixels: number): void {
    // Clear the location where the fiducial marker will be by drawing
    // a white rectangle over the whole area. xPos and yPos are the
    // coordinates of the center of the fiducial marker so we need to
    // offset the coordinates of the rectangle by `fiducialSizePx / 2`
    this.context.fillStyle = WHITE_FILL;
    this.context.fillRect(
      xPos - sizeInPixels / 2,
      yPos - sizeInPixels / 2,
      sizeInPixels,
      sizeInPixels);

    // The outermost circle is inscribed within the square. The other rings are
    // just one cell size less thick than each previous one.
    const outermostCircleRadius = sizeInPixels / 2;
    const intermediateCircleRadius = outermostCircleRadius - this.cellSizePx;
    const innermostCircleRadius = intermediateCircleRadius - this.cellSizePx;

    // Black outer circle, intermediate white circle, inner black circle.
    this.drawCircle(xPos, yPos, outermostCircleRadius, BLACK_FILL);
    this.drawCircle(xPos, yPos, intermediateCircleRadius, WHITE_FILL);
    this.drawCircle(xPos, yPos, innermostCircleRadius, BLACK_FILL);
  }

  // Utility for drawing the actual circles.
  private drawCircle(x: number, y: number, radius: number,
      fillStyle: string): void {
    this.context.fillStyle = fillStyle;
    this.context.beginPath();
    this.context.arc(x, y, radius, 0, 2 * Math.PI);
    this.context.fill();
  }

  // Determines the size of the fiducial markers. The markers are always
  // surrounded by empty cells.
  private determineFiducialSize(): number {
    let fiducialSize = 0;
    for (let index = 0; index < this.cellCount / 2; index++) {
      // Scan the first row (top left marker)
      if (this.qrCode && this.qrCode[index]) {
        fiducialSize++;
      } else {
        return fiducialSize;
      }
    }
    return 0;
  }

  private onResize(): void {
    if (this.qrCode) {
      this.updateInternal();
      this.drawQrCode();
    }
  }
}
