// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 2D point.
 */
export class Point {
  /**
   * @param {number} x
   * @param {number} y
   */
  constructor(x, y) {
    /**
     * @const {number}
     */
    this.x = x;

    /**
     * @const {number}
     */
    this.y = y;
  }
}

const ORIGIN = new Point(0, 0);

/**
 * 2D vector.
 */
export class Vector {
  /**
   * @param {number} x
   * @param {number} y
   */
  constructor(x, y) {
    /**
     * @const {number}
     */
    this.x = x;

    /**
     * @const {number}
     */
    this.y = y;
  }

  /**
   * @return {number}
   */
  length() {
    return Math.hypot(this.x, this.y);
  }

  /**
   * @param {!Vector} v
   * @return {!Vector}
   */
  add(v) {
    return new Vector(this.x + v.x, this.y + v.y);
  }

  /**
   * @param {!Vector} v
   * @return {!Vector}
   */
  minus(v) {
    return new Vector(this.x - v.x, this.y - v.y);
  }

  /**
   * Dot product.
   * @param {!Vector} v
   * @return {number}
   */
  dot(v) {
    return this.x * v.x + this.y * v.y;
  }

  /**
   * Cross product.
   * @param {!Vector} v
   * @return {number}
   */
  cross(v) {
    return this.x * v.y - this.y * v.x;
  }

  /**
   * @param {number} n
   * @return {!Vector}
   */
  multiply(n) {
    return new Vector(this.x * n, this.y * n);
  }

  /**
   * @param {!Vector} v
   * @return {number} Angle required to rotate from this vector to |v|.
   *     Positive/negative sign represent rotating in (counter-)clockwise
   *     direction.
   */
  rotation(v) {
    const cross = this.cross(v);
    const dot = this.dot(v);
    return Math.atan2(cross, dot);
  }

  /**
   * The rotation angle for setting |CSSRotate|.
   * @return {number}
   */
  cssRotateAngle() {
    return ROTATE_START_AXIS.rotation(this);
  }

  /**
   * Unit direction vector.
   * @return {!Vector}
   */
  direction() {
    const length = this.length();
    return new Vector(this.x / length, this.y / length);
  }

  /**
   * Unit normal vector n in direction that the |this| x |n| is positive.
   * @return {!Vector}
   */
  normal() {
    const length = this.length();
    return new Vector(-this.y / length, this.x / length);
  }

  /**
   * @return {!Point}
   */
  point() {
    return new Point(this.x, this.y);
  }
}

/**
 * @param {!Point} end
 * @param {!Point=} start
 * @return {!Vector} Vector points from |start| to |end|.
 */
export function vectorFromPoints(end, start) {
  start = start || ORIGIN;
  return new Vector(end.x - start.x, end.y - start.y);
}

/**
 * Start axis of |CSSRotate|.
 */
const ROTATE_START_AXIS = new Vector(1, 0);

/**
 * Vector with polar representation.
 */
export class PolarVector extends Vector {
  /**
   * @param {number} angle Counter closewise angle start from (1, 0) in rad.
   * @param {number} length
   */
  constructor(angle, length) {
    super(Math.cos(angle) * length, Math.sin(angle) * length);
  }
}
