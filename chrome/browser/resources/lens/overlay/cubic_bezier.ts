// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Point} from './selection_utils.js';

const BEZIER_EPSILON = 1e-7;
const CUBIC_BEZIER_SPLINE_SAMPLES = 11;
const MAX_NEWTON_METHOD_ITERATIONS = 4;

/**
 * This class calculates a cubic bezier easing function with provided parameters
 * to mimic the functionality of the CSS easing function.
 *
 * This implementation is mainly translated from:
 * ui/gfx/geometry/cubic_bezier.h
 */
export class CubicBezier {
  private p1: Point;
  private p2: Point;

  // Polynomial coefficients in form (x,y)
  private a: Point = {x: 0, y: 0};
  private b: Point = {x: 0, y: 0};
  private c: Point = {x: 0, y: 0};

  // Samples for estimating the spline curve
  private splineSamples: number[] = [];

  constructor(x1: number, y1: number, x2: number, y2: number) {
    this.p1 = {x: x1, y: y1};
    this.p2 = {x: x2, y: y2};
    this.initCoefficients(this.p1, this.p2);
    this.initSpline();
  }

  solveForY(x: number): number {
    x = Math.max(0, Math.min(1, x));
    return this.sampleCurveY(this.solveCurveX(x));
  }

  solveCurveX(x: number): number {
    let t0: number|undefined;
    let t1: number|undefined;
    let x2: number|undefined;
    let d2: number;

    let t2 = x;

    // Linear interpolation of spline curve for initial guess.
    const deltaT = 1 / (CUBIC_BEZIER_SPLINE_SAMPLES - 1);
    for (let i = 1; i < CUBIC_BEZIER_SPLINE_SAMPLES; i++) {
      if (x <= this.splineSamples[i]) {
        t1 = deltaT * i;
        t0 = t1 - deltaT;
        t2 = t0 +
            (t1 - t0) * (x - this.splineSamples[i - 1]) /
                (this.splineSamples[i] - this.splineSamples[i - 1]);
        break;
      }
    }

    // Perform a few iterations of Newton's method -- normally very fast.
    // See https://en.wikipedia.org/wiki/Newton%27s_method.
    for (let i = 0; i < MAX_NEWTON_METHOD_ITERATIONS; i++) {
      x2 = this.sampleCurveX(t2) - x;
      if (Math.abs(x2) < BEZIER_EPSILON) {
        return t2;
      }
      d2 = this.sampleCurveDerivativeX(t2);
      if (Math.abs(d2) < BEZIER_EPSILON) {
        break;
      }
      t2 = t2 - x2 / d2;
    }
    if (x2 !== undefined && Math.abs(x2) < BEZIER_EPSILON) {
      return t2;
    }

    // Fall back to the bisection method for reliability.
    if (t0 !== undefined && t1 !== undefined) {
      while (t0 < t1) {
        x2 = this.sampleCurveX(t2);
        if (Math.abs(x2 - x) < BEZIER_EPSILON) {
          return t2;
        }

        if (x > x2) {
          t0 = t2;
        } else {
          t1 = t2;
        }

        t2 = (t1 + t0) * 0.5;
      }
    }

    // Failed to solve.
    return t2;
  }

  sampleCurveX(t: number): number {
    // `ax t^3 + bx t^2 + cx t' expanded using Horner's rule.
    // The x values are in the range [0, 1]. So it isn't needed toFinite
    // clamping.
    // https://drafts.csswg.org/css-easing-1/#funcdef-cubic-bezier-easing-function-cubic-bezier
    return ((this.a.x * t + this.b.x) * t + this.c.x) * t;
  }

  private sampleCurveDerivativeX(t: number) {
    return (3 * this.a.x * t + 2 * this.b.x) * t + this.c.x;
  }


  private sampleCurveY(t: number): number {
    return this.toFinite(((this.a.y * t + this.b.y) * t + this.c.y) * t);
  }

  private initCoefficients(p1: Point, p2: Point) {
    // Calculate the polynomial coefficients, implicit first and last control
    // points are (0,0) and (1,1). First, for x.
    this.c.x = 3 * p1.x;
    this.b.x = 3 * (p2.x - p1.x) - this.c.x;
    this.a.x = 1 - this.c.x - this.b.x;

    // Now for y.
    this.c.y = this.toFinite(3 * p1.y);
    this.b.y = this.toFinite(3 * (p2.y - p1.y) - this.c.y);
    this.a.y = this.toFinite(1 - this.c.y - this.b.y);
  }

  private initSpline() {
    const deltaT = 1 / (CUBIC_BEZIER_SPLINE_SAMPLES - 1);
    for (let i = 0; i < CUBIC_BEZIER_SPLINE_SAMPLES; i++) {
      this.splineSamples[i] = this.sampleCurveX(i * deltaT);
    }
  }

  private toFinite(n: number): number {
    if (Number.isFinite(n)) {
      return n;
    }

    return n > 0 ? Number.MAX_SAFE_INTEGER : Number.MIN_SAFE_INTEGER;
  }
}
