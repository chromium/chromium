// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class Coordinate2d {
  private x_: number;
  private y_: number;

  /**
   * Immutable two dimensional point in space. The units of the dimensions are
   * undefined.
   * @param x X-dimension of the point.
   * @param y Y-dimension of the point.
   */
  constructor(x: number, y: number) {
    this.x_ = x;
    this.y_ = y;
  }

  get x(): number {
    return this.x_;
  }

  get y(): number {
    return this.y_;
  }

  equals(other: Coordinate2d): boolean {
    return other !== null && this.x_ === other.x_ && this.y_ === other.y_;
  }
}
