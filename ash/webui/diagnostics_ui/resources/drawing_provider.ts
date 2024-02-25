// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {constructRgba, DESTINATION_OVER, getTrailOpacityFromPressure, LINE_CAP, LINE_WIDTH, lookupCssVariableValue, MARK_COLOR, MARK_OPACITY, MARK_RADIUS, SOURCE_OVER, TRAIL_COLOR} from './drawing_provider_utils.js';


/**
 * DrawingProvider interface provides drawing methods for touchscreen and
 * touchpad tester to draw on screen.
 */
interface DrawingProvider {
  /**
   * For touchscreen tester to draw a trail on screen. A trail is connected by
   * multiple lines. This function essentially draws a line by connecting two
   * points.
   * @param x0 The x coordinate of the first point.
   * @param y0 The y coordinate of the first point.
   * @param x1 The x coordinate of the second point.
   * @param y1 The y coordinate of the second point.
   * @param pressure The pressure of the touch to indicate the opacity of the
   *     trail.
   */
  drawTrail(x0: number, y0: number, x1: number, y1: number, pressure: number):
      void;

  /**
   * For touchscreen tester to draw a trail starting or ending mark on screen.
   * A mark is essentially a circle on screen.
   * @param x The x coordinate of the circle center.
   * @param y The y coordinate of the circle center.
   */
  drawTrailMark(x: number, y: number): void;
}

/**
 * CanvasDrawingProvider implements the DrawingProvider interface using html
 * Canvas API. This design makes the drawing mechanism in a replaceable module.
 */
export class CanvasDrawingProvider implements DrawingProvider {
  private ctx: CanvasRenderingContext2D;

  constructor(ctx: CanvasRenderingContext2D) {
    this.ctx = ctx;
    this.setup();
  }

  setup(): void {
    assert(this.ctx);
    this.ctx.lineCap = LINE_CAP;
    this.ctx.lineWidth = LINE_WIDTH;
  }

  /**
   * For testing only.
   */
  getCtx(): CanvasRenderingContext2D {
    return this.ctx;
  }

  /**
   * For testing only.
   */
  getLineCap(): string {
    return this.ctx.lineCap;
  }

  /**
   * For testing only.
   */
  getLineWidth(): number {
    return this.ctx.lineWidth;
  }

  /**
   * For testing only.
   */
  getStrokeStyle(): string|CanvasGradient|CanvasPattern {
    return this.ctx.strokeStyle;
  }

  /**
   * For testing only.
   */
  getFillStyle(): string|CanvasGradient|CanvasPattern {
    return this.ctx.fillStyle;
  }

  /**
   * For testing only.
   */
  getGlobalCompositeOperation(): string {
    return this.ctx.globalCompositeOperation;
  }

  /**
   * Draw a line on canvas.
   */
  drawTrail(x0: number, y0: number, x1: number, y1: number, pressure: number):
      void {
    assert(this.ctx);
    this.ctx.strokeStyle = constructRgba(
        lookupCssVariableValue(TRAIL_COLOR),
        getTrailOpacityFromPressure(pressure));
    this.ctx.beginPath();
    this.ctx.moveTo(x0, y0);
    this.ctx.lineTo(x1, y1);
    this.ctx.stroke();
  }

  /**
   * Draw a trail mark on canvas.
   */
  drawTrailMark(x: number, y: number): void {
    assert(this.ctx);
    // Making sure the mark is always on top.
    this.ctx.globalCompositeOperation = SOURCE_OVER;
    this.ctx.fillStyle = constructRgba(
        lookupCssVariableValue(MARK_COLOR),
        lookupCssVariableValue(MARK_OPACITY));
    this.ctx.beginPath();
    this.ctx.arc(x, y, MARK_RADIUS, 0, 2 * Math.PI);
    this.ctx.fill();
    this.ctx.globalCompositeOperation = DESTINATION_OVER;
  }
}
