// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MAX_LABEL_VERTICAL_NUM, MIN_LABEL_VERTICAL_SPACING, TEXT_SIZE} from '../configs.js';

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

  // The current max value for this label. To calculate the suitable units.
  private maxValue: number = 0;
  // The cache of maxValue. See `setMaxValue()`.
  private maxValueCache: number = 0;
  // The current suitable unit's index.
  private currentUnitIdx: number = 0;
  // The generated text labels.
  private labels: string[] = [];
  // The height of the label, in pixels.
  private height: number = 0;
  // The maximum precision for the number of the label.
  private precision: number = 1;
  // Vertical scale of line chart. The real value between two pixels.
  private valueScale: number = 1;
  // True if the label need not be regenerated.
  private isCache: boolean = false;

  /**
   * Get the generated text labels.
   */
  getLabels(): string[] {
    this.updateLabelsAndScale();
    return this.labels;
  }

  /**
   * Get the vertical scale of line chart.
   */
  getValueScale(): number {
    this.updateLabelsAndScale();
    return this.valueScale;
  }

  /**
   * Set the layout of the label.
   * @param height - The label height, in pixels.
   * @param precision - The maximum precision of the value of label.
   *      It means that the minimum step size of the label is `10^(-percision)`.
   */
  setLayout(height: number, precision: number) {
    if (precision < 0 || precision > 20) {
      console.warn('Precision must be between 0 and 20.');
      return;
    }
    if (this.height === height && this.precision === precision) {
      return;
    }

    this.height = height;
    this.precision = precision;
    this.isCache = false;
  }

  /**
   * Set the maximum value of the label. Decide the suitable unit by this value.
   */
  setMaxValue(maxValue: number) {
    if (this.maxValueCache === maxValue) {
      return;
    }
    this.maxValueCache = maxValue;

    const result: {unitIdx: number, value: number} =
        this.getSuitableUnit(maxValue);
    this.currentUnitIdx = result.unitIdx;
    this.maxValue = result.value;
    this.isCache = false;
  }

  /**
   * Find the suitable unit for the original value. If the value is greater than
   * `unitBase`, we will try to use a bigger unit.
   */
  private getSuitableUnit(value: number): {unitIdx: number, value: number} {
    let unitIdx: number = 0;
    while (unitIdx + 1 < this.units.length && value >= this.unitBase) {
      value /= this.unitBase;
      ++unitIdx;
    }
    return {
      unitIdx: unitIdx,
      value: value,
    };
  }

  /**
   * Update the labels and scale if the status is changed.
   */
  private updateLabelsAndScale() {
    if (this.isCache) {
      return;
    }
    this.isCache = true;

    if (this.maxValue === 0) {
      return;
    }

    const result: {stepSize: number, stepSizePrecision: number} =
        this.getSuitableStepSize();
    const stepSize: number = result.stepSize;
    const stepSizePrecision: number = result.stepSizePrecision;

    const topLabelValue: number =
        this.getTopLabelValue(this.maxValue, stepSize);
    const unitStr: string = this.getCurrentUnitString();
    const labels: string[] = [];
    for (let value: number = topLabelValue; value >= 0; value -= stepSize) {
      const valueStr: string = value.toFixed(stepSizePrecision);
      const label: string = valueStr + ' ' + unitStr;
      labels.push(label);
    }
    this.labels = labels;

    const realTopValue = this.getRealValueWithCurrentUnit(topLabelValue);
    this.valueScale = realTopValue / this.height;
  }

  /**
   * Top label value is an exact multiple of `stepSize`.
   */
  private getTopLabelValue(maxValue: number, stepSize: number): number {
    return Math.ceil(maxValue / stepSize) * stepSize;
  }

  /**
   * Get current suitable unit.
   */
  private getCurrentUnitString(): string {
    return this.units[this.currentUnitIdx];
  }

  /**
   * Transform the value in the current suitable unit to the real value.
   */
  private getRealValueWithCurrentUnit(value: number): number {
    return value * Math.pow(this.unitBase, this.currentUnitIdx);
  }

  /**
   * Find a step size to show a suitable amount of labels on screen. The default
   * step size according to the precision of the label. We will try 1 time, 2
   * times and 5 tims of the default step size. If they are not suitable, we
   * will reduce the precision and try again.
   */
  private getSuitableStepSize(): {stepSize: number, stepSizePrecision: number} {
    const maxLabelNum: number = this.getMaxNumberOfLabel();
    let stepSize = Math.pow(10, -this.precision);

    // This number is for Number.toFixed. if precision is less than 0, it is set
    // to 0.
    let stepSizePrecision: number = Math.max(this.precision, 0);
    while (true) {
      if (this.getNumberOfLabelWithStepSize(stepSize) <= maxLabelNum) {
        break;
      }
      if (this.getNumberOfLabelWithStepSize(stepSize * 2) <= maxLabelNum) {
        stepSize *= 2;
        break;
      }
      if (this.getNumberOfLabelWithStepSize(stepSize * 5) <= maxLabelNum) {
        stepSize *= 5;
        break;
      }

      /* Reduce the precision. */
      stepSize *= 10;
      if (stepSizePrecision > 0) {
        --stepSizePrecision;
      }
    }

    return {stepSize: stepSize, stepSizePrecision: stepSizePrecision};
  }

  /**
   * Get the maximum number of equally spaced labels. `TEXT_SIZE` is doubled
   * because the top two labels are both drawn in the same gap.
   */
  private getMaxNumberOfLabel(): number {
    const minLabelSpacing: number = 2 * TEXT_SIZE + MIN_LABEL_VERTICAL_SPACING;
    const maxLabelNum: number = 1 + Math.floor(this.height / minLabelSpacing);
    return Math.min(Math.max(maxLabelNum, 2), MAX_LABEL_VERTICAL_NUM);
  }

  /**
   * Get the number of labels with `stepSize`. Because we want the top of the
   * label to be an exact multiple of the `stepSize`, we use `Math.ceil() + 1`
   * to add an additional label above the `maxValue`.
   */
  private getNumberOfLabelWithStepSize(stepSize: number): number {
    return Math.ceil(this.maxValue / stepSize) + 1;
  }
}
