// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Colors for drawing.
const BLACK_FILL = '#202124';
const WHITE_FILL = '#ffffff';

// Default size to use for the canvas on first draw, before the resize event.
const DEFAULT_SIZE = 400;

// When drawing circles, use this value to determine how big the radius of
// the small dots should be.
const RADIUS_PERCENTAGE = 0.75;

//  QrCodeCanvas
//  ----------------
//  Utility for drawing QR codes using circles or squares. Currently used by
//  QuickStart. The QR code input data contains a bit for each cell dictating
//  whether or not it should be drawn.
//
//     ___________ ...   Each of the square cells of the QR code will be drawn
//    |    |    |        using `CELL_SIZE_PX` pixels. The circles are then
//    |____|____|_ ...   embedded into these squares with their centers matching
//    |    |    |        the square's center. The radius of the circles is
//    :    :    :        controlled by `RADIUS_PERCENTAGE`.
//
//   A ResizeObserver is attached to the given <canvas> and monitors any size
//   changes, triggering a redraw with optimal sizes based on the screen's DPI.
//
export class QrCodeCanvas {
  constructor(canvas) {
    this.canvas_ = canvas;
    this.context_ = canvas.getContext('2d');

    // Observe size changes.
    this.observer_ = new ResizeObserver(this.onResize.bind(this));
    this.observer_.observe(this.canvas_);

    // Default values
    this.qrCode_ = null;
    this.useCircles = true;
    this.canvasSize_ = 0;
    this.CELL_COUNT = 0;
    this.CELL_SIZE_PX = 0;
    this.DOT_RADIUS_PX = 0;
    this.FIDUCIAL_CELL_COUNT = 0;
  }

  setData(qrData) {
    this.qrCode_ = qrData;
    this.updateInternal();
    this.drawQrCode();
  }

  updateInternal() {
    // The true size of the canvas on screen right now.
    const canvasSizeOnScreen = Math.ceil(this.canvas_.clientHeight);

    // Ideal size used for drawing, taking into account the screen's DPI.
    // If the canvas has no size yet, use a default value until resize occurs.
    const canvasSize = canvasSizeOnScreen > 0 ?
      canvasSizeOnScreen * window.devicePixelRatio * 2 :
      DEFAULT_SIZE;
    this.canvasSize_ = canvasSize;
    this.canvas_.height = this.canvas_.width = this.canvasSize_;

    this.CELL_COUNT = Math.round(Math.sqrt(this.qrCode_.length));
    this.CELL_SIZE_PX = this.canvasSize_ / this.CELL_COUNT;
    this.DOT_RADIUS_PX = (this.CELL_SIZE_PX / 2) * RADIUS_PERCENTAGE;
    this.FIDUCIAL_CELL_COUNT = this.determineFiducialSize();
    if (this.FIDUCIAL_CELL_COUNT == 0) {
      // Could not determine. Default to drawing squares.
      this.useCircles = false;
    }

    // Add CELL_COUNT to the <canvas> as an attribute for browsertests.
    this.canvas_.qrCellCount = this.CELL_COUNT;
  }

  toggleMode() {
    this.useCircles = !this.useCircles;
    this.updateInternal();
    this.drawQrCode();
  }

  drawQrCode() {
    // Draw all of the cells of the QR code.
    this.drawDataPoints();

    // Redraw the fiducial markers as circles if necessary.
    if (this.useCircles) {
      // Draw fiducials
      const fiducialSizePx = this.FIDUCIAL_CELL_COUNT * this.CELL_SIZE_PX;
      const fiducialRadiusPx = fiducialSizePx / 2;

      // Upper left.
      this.drawFiducial(
        /* xPos= */ 0 + fiducialRadiusPx,
        /* yPos= */ 0 + fiducialRadiusPx,
        fiducialSizePx);

      // Upper right
      this.drawFiducial(
        /* xPos= */ this.canvasSize_ - fiducialRadiusPx,
        /* yPos= */ 0 + fiducialRadiusPx,
        fiducialSizePx);

      // Lower left
      this.drawFiducial(
        /* xPos= */ 0 + fiducialRadiusPx,
        /* yPos= */ this.canvasSize_ - fiducialRadiusPx,
        fiducialSizePx);
    }

  }

  drawDataPoints() {
    this.context_.clearRect(0, 0, this.canvasSize_, this.canvasSize_);
    this.context_.fillStyle = BLACK_FILL;

    // Offset the circles by half of the tile size in order to draw them
    // in the center of the tile. Without this, the circles at the upper
    // and left edges of the code would consist of half circles instead of
    // full circles.
    const OFFSET = this.CELL_SIZE_PX / 2;

    for (let y = 0; y < this.CELL_COUNT; y++) {
      for (let x = 0; x < this.CELL_COUNT; x++) {
        const index = y * this.CELL_COUNT + x;
        if (this.qrCode_[index]) {
          if (this.useCircles) {
            this.context_.beginPath();
            this.context_.arc(
              x * this.CELL_SIZE_PX + OFFSET,
              y * this.CELL_SIZE_PX + OFFSET,
              this.DOT_RADIUS_PX, 0, 2 * Math.PI);
            this.context_.fill();
          } else {
            this.context_.fillRect(
              x * this.CELL_SIZE_PX,
              y * this.CELL_SIZE_PX,
              this.CELL_SIZE_PX, this.CELL_SIZE_PX);
          }
        }
      }
    }
  }

  // Draw a circular fiducial marker at the given location. It consists
  // of 3 concentric circles whereas each circle is smaller than the
  // previous one by the size of one cell.
  drawFiducial(xPos, yPos, sizeInPixels) {
    // Clear the location where the fiducial marker will be by drawing
    // a white rectangle over the whole area. xPos and yPos are the
    // coordinates of the center of the fiducial marker so we need to
    // offset the coordinates of the rectangle by `fiducialSizePx / 2`
    this.context_.fillStyle = WHITE_FILL;
    this.context_.fillRect(
      xPos - sizeInPixels / 2,
      yPos - sizeInPixels / 2,
      sizeInPixels,
      sizeInPixels);

    // The outermost circle is inscribed within the square. The other rings are
    // just one cell size less thick than each previous one.
    const outermostCircleRadius = sizeInPixels / 2;
    const intermediateCircleRadius = outermostCircleRadius - this.CELL_SIZE_PX;
    const innermostCircleRadius = intermediateCircleRadius - this.CELL_SIZE_PX;

    // Black outer circle, intermediate white circle, inner black circle.
    this.drawCircle(xPos, yPos, outermostCircleRadius, BLACK_FILL);
    this.drawCircle(xPos, yPos, intermediateCircleRadius, WHITE_FILL);
    this.drawCircle(xPos, yPos, innermostCircleRadius, BLACK_FILL);
  }

  // Utility for drawing the actual circles.
  drawCircle(x, y, radius, fillStyle) {
    this.context_.fillStyle = fillStyle;
    this.context_.beginPath();
    this.context_.arc(x, y, radius, 0, 2 * Math.PI);
    this.context_.fill();
  }

  // Determines the size of the fiducial markers. The markers are always
  // surrounded by empty cells.
  determineFiducialSize() {
    let fiducialSize = 0;
    for (let index = 0; index < this.CELL_COUNT / 2; index++) {
      // Scan the first row (top left marker)
      if (this.qrCode_[index]) {
        fiducialSize++;
      } else {
        return fiducialSize;
      }
    }
    return 0;
  }

  onResize() {
    if (this.qrCode_) {
      this.updateInternal();
      this.drawQrCode();
    }
  }
}
