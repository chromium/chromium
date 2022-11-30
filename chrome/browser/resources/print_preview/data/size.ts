// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class Size {
  private width_: number;
  private height_: number;

  /**
   * Immutable two-dimensional size.
   */
  constructor(width: number, height: number) {
    this.width_ = width;
    this.height_ = height;
  }

  get width(): number {
    return this.width_;
  }

  get height(): number {
    return this.height_;
  }

  equals(other: Size): boolean {
    return other !== null && this.width_ === other.width_ &&
        this.height_ === other.height_;
  }
}
