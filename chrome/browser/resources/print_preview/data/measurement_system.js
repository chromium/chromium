// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

/**
 * Enumeration of measurement unit types.
 * @enum {number}
 */
export const MeasurementSystemUnitType = {
  METRIC: 0,   // millimeters
  IMPERIAL: 1  // inches
};

/**
 * @typedef {{precision: number,
 *            decimalPlaces: number,
 *            ptsPerUnit: number,
 *            unitSymbol: string}}
 */
let MeasurementSystemPrefs;

export class MeasurementSystem {
  /**
   * Measurement system of the print preview. Used to parse and serialize
   * point measurements into the system's local units (e.g. millimeters,
   * inches).
   * @param {string} thousandsDelimiter Delimiter between thousands digits.
   * @param {string} decimalDelimiter Delimiter between integers and decimals.
   * @param {!MeasurementSystemUnitType} unitType Measurement
   *     unit type of the system.
   */
  constructor(thousandsDelimiter, decimalDelimiter, unitType) {
    /**
     * The thousands delimiter to use when displaying numbers.
     * @private {string}
     */
    this.thousandsDelimiter_ = thousandsDelimiter || ',';

    /**
     * The decimal delimiter to use when displaying numbers.
     * @private {string}
     */
    this.decimalDelimiter_ = decimalDelimiter || '.';

    assert(measurementSystemPrefs.has(unitType));
    /**
     * The measurement system preferences based on the unit type.
     * @private {!MeasurementSystemPrefs}
     */
    this.measurementSystemPrefs_ = measurementSystemPrefs.get(unitType);
  }

  /** @return {string} The unit type symbol of the measurement system. */
  get unitSymbol() {
    return this.measurementSystemPrefs_.unitSymbol;
  }

  /**
   * @return {string} The thousands delimiter character of the measurement
   *     system.
   */
  get thousandsDelimiter() {
    return this.thousandsDelimiter_;
  }

  /**
   * @return {string} The decimal delimiter character of the measurement
   *     system.
   */
  get decimalDelimiter() {
    return this.decimalDelimiter_;
  }

  /**
   * Rounds a value in the local system's units to the appropriate precision.
   * @param {number} value Value to round.
   * @return {number} Rounded value.
   */
  roundValue(value) {
    const precision = this.measurementSystemPrefs_.precision;
    const roundedValue = Math.round(value / precision) * precision;
    // Truncate
    return +roundedValue.toFixed(this.measurementSystemPrefs_.decimalPlaces);
  }

  /**
   * @param {number} pts Value in points to convert to local units.
   * @return {number} Value in local units.
   */
  convertFromPoints(pts) {
    return pts / this.measurementSystemPrefs_.ptsPerUnit;
  }

  /**
   * @param {number} localUnits Value in local units to convert to points.
   * @return {number} Value in points.
   */
  convertToPoints(localUnits) {
    return localUnits * this.measurementSystemPrefs_.ptsPerUnit;
  }
}

/**
 * Maximum resolution and number of decimal places for local unit values.
 * @private {!Map<!MeasurementSystemUnitType,
 *                !MeasurementSystemPrefs>}
 */
const measurementSystemPrefs = new Map([
  [
    MeasurementSystemUnitType.METRIC, {
      precision: 0.5,
      decimalPlaces: 1,
      ptsPerUnit: 72.0 / 25.4,
      unitSymbol: 'mm'
    }
  ],
  [
    MeasurementSystemUnitType.IMPERIAL,
    {precision: 0.01, decimalPlaces: 2, ptsPerUnit: 72.0, unitSymbol: '"'}
  ]
]);
