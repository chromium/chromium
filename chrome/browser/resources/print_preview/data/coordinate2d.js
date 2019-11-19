// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class Coordinate2d {
  /**
   * Immutable two dimensional point in space. The units of the dimensions are
   * undefined.
   * @param {number} x X-dimension of the point.
   * @param {number} y Y-dimension of the point.
   */
  constructor(x, y) {
    /**
     * X-dimension of the point.
     * @type {number}
     * @private
     */
    this.x_ = x;

    /**
     * Y-dimension of the point.
     * @type {number}
     * @private
     */
    this.y_ = y;
  }

  /** @return {number} X-dimension of the point. */
  get x() {
    return this.x_;
  }

  /** @return {number} Y-dimension of the point. */
  get y() {
    return this.y_;
  }

  /**
   * @param {Coordinate2d} other The point to compare against.
   * @return {boolean} Whether another point is equal to this one.
   */
  equals(other) {
    return other != null && this.x_ == other.x_ && this.y_ == other.y_;
  }
}
