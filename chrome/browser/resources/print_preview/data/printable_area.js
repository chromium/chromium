// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Coordinate2d} from './coordinate2d.js';
import {Size} from './size.js';

export class PrintableArea {
  /**
   * Object describing the printable area of a page in the document.
   * @param {!Coordinate2d} origin Top left corner of the
   *     printable area of the document.
   * @param {!Size} size Size of the printable area of the
   *     document.
   */
  constructor(origin, size) {
    /**
     * Top left corner of the printable area of the document.
     * @type {!Coordinate2d}
     * @private
     */
    this.origin_ = origin;

    /**
     * Size of the printable area of the document.
     * @type {!Size}
     * @private
     */
    this.size_ = size;
  }

  /**
   * @return {!Coordinate2d} Top left corner of the printable
   *     area of the document.
   */
  get origin() {
    return this.origin_;
  }

  /**
   * @return {!Size} Size of the printable area of the document.
   */
  get size() {
    return this.size_;
  }

  /**
   * @param {PrintableArea} other Other printable area to check
   *     for equality.
   * @return {boolean} Whether another printable area is equal to this one.
   */
  equals(other) {
    return other != null && this.origin_.equals(other.origin_) &&
        this.size_.equals(other.size_);
  }
}
