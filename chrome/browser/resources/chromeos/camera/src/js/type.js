// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/* eslint-disable no-unused-vars */

/**
 * Photo or video resolution.
 */
var Resolution = class {
  /**
   * @param {number} width
   * @param {number} height
   */
  constructor(width, height) {
    /**
     * @type {number}
     * @const
     */
    this.width = width;

    /**
     * @type {number}
     * @const
     */
    this.height = height;
  }

  /**
   * @return {number} Total pixel number.
   */
  get area() {
    return this.width * this.height;
  }

  /**
   * Aspect ratio calculates from width divided by height.
   * @return {number}
   */
  get aspectRatio() {
    // Approxitate to 4 decimal places to prevent precision error during
    // comparing.
    return parseFloat((this.width / this.height).toFixed(4));
  }

  /**
   * Compares width/height of resolutions, see if they are equal or not.
   * @param {!Resolution} resolution Resolution to be compared with.
   * @return {boolean} Whether width/height of resolutions are equal.
   */
  equals(resolution) {
    return this.width === resolution.width && this.height === resolution.height;
  }

  /**
   * Compares aspect ratio of resolutions, see if they are equal or not.
   * @param {!Resolution} resolution Resolution to be compared with.
   * @return {boolean} Whether aspect ratio of resolutions are equal.
   */
  aspectRatioEquals(resolution) {
    return this.width * resolution.height === this.height * resolution.width;
  }

  /**
   * Create Resolution object from string.
   * @param {string} s
   * @return {!Resolution}
   */
  static fromString(s) {
    return new Resolution(...s.split('x').map(Number));
  }

  /**
   * @override
   */
  toString() {
    return `${this.width}x${this.height}`;
  }
};

/**
 * @typedef {{
 *   width: number,
 *   height: number,
 *   maxFps: number,
 * }}
 */
var VideoConfig;

/**
 * @typedef {{
 *   minFps: number,
 *   maxFps: number,
 * }}
 */
var FpsRange;

/**
 * A list of resolutions.
 * @typedef {Array<!Resolution>}
 */
var ResolutionList;

/**
 * Map of all available resolution to its maximal supported capture fps. The key
 * of the map is the resolution and the corresponding value is the maximal
 * capture fps under that resolution.
 * @typedef {Object<(!Resolution|string), number>}
 */
var MaxFpsInfo;

/**
 * List of supported capture fps ranges.
 * @typedef {Array<!FpsRange>}
 */
var FpsRangeList;

/* eslint-enable no-unused-vars */
