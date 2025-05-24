// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Coordinate2d} from './coordinate2d.js';
import type {Size} from './size.js';

export class PrintableArea {
  private origin_: Coordinate2d;
  private size_: Size;

  /**
   * Object describing the printable area of a page in the document.
   * @param origin Top left corner of the printable area of the document.
   * @param size Size of the printable area of the document.
   */
  constructor(origin: Coordinate2d, size: Size) {
    this.origin_ = origin;
    this.size_ = size;
  }

  get origin(): Coordinate2d {
    return this.origin_;
  }

  get size(): Size {
    return this.size_;
  }

  equals(other: PrintableArea): boolean {
    return other !== null && this.origin_.equals(other.origin_) &&
        this.size_.equals(other.size_);
  }
}
