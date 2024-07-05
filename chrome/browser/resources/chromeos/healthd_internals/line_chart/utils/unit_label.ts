// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A scalable label which can calculate the suitable unit and generate text
 * labels.
 */
export class UnitLabel {
  constructor(units: string[], unitBase: number) {
    this.units = units;
    if (units.length === 0) {
      console.error(
          'LineChart.UnitLabel: Length of units must greater than 0.');
    }

    this.unitBase = unitBase;
    if (unitBase <= 0) {
      console.error('LineChart.UnitLabel: unitBase must greater than 0.');
    }
  }

  // The unit set. E.g. ['B', 'KB', 'MB'].
  private readonly units: string[];
  // The base of the units. It means the next unit is `unitBase` times of
  // current unit. E.g. The `unitBase` of ['B', 'KB', 'MB'] is 1024.
  private readonly unitBase: number;
}
