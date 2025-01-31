// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enumeration of the orientations of margins.
 */
export enum CustomMarginsOrientation {
  TOP = 'top',
  RIGHT = 'right',
  BOTTOM = 'bottom',
  LEFT = 'left',
}

/**
 * Must be kept in sync with the C++ MarginType enum in
 * printing/print_job_constants.h.
 */
export enum MarginsType {
  DEFAULT = 0,
  NO_MARGINS = 1,
  MINIMUM = 2,
  CUSTOM = 3,
}

/**
 * Keep in sync with the C++ kSettingMargin... values in
 * printing/print_job_constants.h.
 */
export interface MarginsSetting {
  marginTop: number;
  marginRight: number;
  marginBottom: number;
  marginLeft: number;
}

type MarginsObject = {
  [K in CustomMarginsOrientation]: number
};

export class Margins {
  /**
   * Backing store for the margin values in points. The numbers are stored as
   * integer values, because that is what the C++ `printing::PageMargins` class
   * expects.
   */
  private value_: MarginsObject = {top: 0, bottom: 0, left: 0, right: 0};

  /**
   * Creates a Margins object that holds four margin values in points.
   */
  constructor(top: number, right: number, bottom: number, left: number) {
    this.value_ = {
      top: Math.round(top),
      right: Math.round(right),
      bottom: Math.round(bottom),
      left: Math.round(left),
    };
  }

  /**
   * @param orientation Specifies the margin value to get.
   * @return Value of the margin of the given orientation.
   */
  get(orientation: CustomMarginsOrientation): number {
    return this.value_[orientation];
  }

  /**
   * @param orientation Specifies the margin to set.
   * @param value Updated value of the margin in points to modify.
   * @return A new copy of |this| with the modification made to the specified
   *     margin.
   */
  set(orientation: CustomMarginsOrientation, value: number): Margins {
    const newValue = this.clone_();
    newValue[orientation] = value;
    return new Margins(
        newValue[CustomMarginsOrientation.TOP],
        newValue[CustomMarginsOrientation.RIGHT],
        newValue[CustomMarginsOrientation.BOTTOM],
        newValue[CustomMarginsOrientation.LEFT]);
  }

  /**
   * @param other The other margins object to compare against.
   * @return Whether this margins object is equal to another.
   */
  equals(other: Margins|null): boolean {
    if (other === null) {
      return false;
    }
    for (const key in this.value_) {
      const orientation = key as CustomMarginsOrientation;
      if (this.value_[orientation] !== other.value_[orientation]) {
        return false;
      }
    }
    return true;
  }

  /** @return A serialized representation of the margins. */
  serialize(): MarginsObject {
    return this.clone_();
  }

  private clone_(): MarginsObject {
    const clone: MarginsObject = {top: 0, bottom: 0, left: 0, right: 0};
    for (const o in this.value_) {
      const orientation = o as CustomMarginsOrientation;
      clone[orientation] = this.value_[orientation];
    }
    return clone;
  }
}
