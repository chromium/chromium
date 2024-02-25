// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MAX_VERTICAL_LABEL_NUM, MIN_LABEL_VERTICAL_SPACING} from './constants.js';

/**
 * Create by |LineChart.LineChart|.
 * A scalable label which can calculate the suitable unit and generate text
 * labels.
 * @const
 */
export class UnitLabel {
  constructor(/** Array<string> */ units, /** number */ unitBase) {
    /** @const {Array<string>} - See |getSuitableUnit()|. */
    this.units_ = units;
    if (units.length === 0) {
      console.warn('LineChart.UnitLabel: Length of units must greater than 0.');
    }

    /** @const {number} - See |getSuitableUnit()|. */
    this.unitBase_ = unitBase;
    if (unitBase < 0) {
      console.warn('LineChart.UnitLabel: unitBase must greater than 0.');
    }

    /**
     * The current max value for this label. To calculate the suitable units.
     * @type {number}
     */
    this.maxValue_ = 0;

    /** @type {number} - The cache of maxValue. See |setMaxValue()|. */
    this.maxValueCache_ = 0;

    /** @type {number} - The current suitable unit's index. */
    this.currentUnitIdx_ = 0;

    /** @type {Array<string>} - The generated text labels. */
    this.labels_ = [];

    /** @type {number} - The height of the label, in pixels. */
    this.height_ = 0;

    /** @type {number} - The font height of the label, in pixels. */
    this.fontHeight_ = 1;

    /** @type {number} - The maximum precision for the number of the label. */
    this.precision_ = 1;

    /** @type {number} - See |getScale()|. */
    this.scale_ = 1;

    /** @type {boolean} True if the label need not be regenerated. */
    this.isCache_ = false;
  }

  /**
   * Get the generated text labels.
   * @return {Array<string>}
   */
  getLabels() {
    this.updateLabelsAndScale_();
    return this.labels_;
  }

  /**
   * The scale of the real value and the y coordinate of the chart.
   * @return {number}
   */
  getScale() {
    this.updateLabelsAndScale_();
    return this.scale_;
  }

  /**
   * Get current suitable unit.
   * @return {string}
   */
  getCurrentUnitString() {
    return this.units_[this.currentUnitIdx_];
  }

  /**
   * Set the layout of the label. See |LineChart.SubChart.setLayout()|.
   * @param {number} height - The label height, in pixels.
   * @param {number} fontHeight - The font height, in pixels.
   * @param {number} precision - The maximum precision of the value of label.
   *      It means that the minimum step size of the label is |10^(-percision)|.
   */
  setLayout(height, fontHeight, precision) {
    if (precision < 0 || precision > 20) {
      console.warn('Precision must be between 0 and 20.');
      return;
    }
    if (this.height_ === height && this.fontHeight_ === fontHeight &&
        this.precision_ === precision) {
      return;
    }

    this.height_ = height;
    this.fontHeight_ = fontHeight;
    this.precision_ = precision;
    this.isCache_ = false;
  }

  /**
   * Set the maximum value of the label. Decide the suitable unit by this value.
   * @param {number} maxValue
   */
  setMaxValue(maxValue) {
    if (this.maxValueCache_ === maxValue) {
      return;
    }
    this.maxValueCache_ = maxValue;

    const /** Array<string> */ units = this.units_;
    const /** number */ unitBase = this.unitBase_;
    const /** {unitIdx: number, value: number} */ result =
        this.constructor.getSuitableUnit(maxValue, units, unitBase);
    this.currentUnitIdx_ = result.unitIdx;
    this.maxValue_ = result.value;
    this.isCache_ = false;
  }

  /**
   * Find the suitable unit. If the value is greater than |unitBase|, we will
   * try to use a bigger unit.
   * @param {number} value - The original value.
   * @param {Array<string>} units - The unit set. Ex: ['B', 'KB', 'MB'].
   * @param {number} unitBase - The base of the units. It means the next unit
   *     is |unitBase| times of current unit.
   *     Ex: The |unitBase| of ['B', 'KB', 'MB'] is 1024.
   *
   * @return {{unitIdx: number, value: number}}
   */
  static getSuitableUnit(value, units, unitBase) {
    let /** number */ unitIdx = 0;
    while (unitIdx + 1 < units.length && value >= unitBase) {
      value /= unitBase;
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
  updateLabelsAndScale_() {
    if (this.isCache_) {
      return;
    }
    this.isCache_ = true;

    if (this.maxValue_ === 0) {
      return;
    }

    const /** {stepSize: number, stepSizePrecision: number} */ result =
        this.getSuitableStepSize_();
    const /** number */ stepSize = result.stepSize;
    const /** number */ stepSizePrecision = result.stepSizePrecision;

    const /** number */ topLabelValue =
        this.getTopLabelValue_(this.maxValue_, stepSize);
    const /** string */ unitStr = this.getCurrentUnitString();
    const /** Array<string> */ labels = [];
    for (let /** number */ value = topLabelValue; value >= 0;
         value -= stepSize) {
      const /** string */ valueStr = value.toFixed(stepSizePrecision);
      const /** string */ label = valueStr + ' ' + unitStr;
      labels.push(label);
    }
    this.labels_ = labels;

    const /** number */ realTopValue =
        this.getRealValueWithCurrentUnit_(topLabelValue);
    this.scale_ = this.height_ / realTopValue;
  }

  /**
   * Find a step size to show a suitable amount of labels on screen. The default
   * step size according to the precision of the label. We will try 1 time, 2
   * times and 5 tims of the default step size. If they are not suitable, we
   * will reduce the precision and try again.
   * @return {{stepSize: number, stepSizePrecision: number}}
   */
  getSuitableStepSize_() {
    const /** number */ maxValue = this.maxValue_;
    const /** number */ maxLabelNum = this.getMaxNumberOfLabel_();
    let /** number */ stepSize = Math.pow(10, -this.precision_);

    /**
     * This number is for Number.toFixed. if precision is less than 0, it is set
     * to 0.
     * @type {number}
     */
    let stepSizePrecision = Math.max(this.precision_, 0);
    while (true) {
      if (this.getNumberOfLabelWithStepSize_(stepSize) <= maxLabelNum) {
        break;
      }
      if (this.getNumberOfLabelWithStepSize_(stepSize * 2) <= maxLabelNum) {
        stepSize *= 2;
        break;
      }
      if (this.getNumberOfLabelWithStepSize_(stepSize * 5) <= maxLabelNum) {
        stepSize *= 5;
        break;
      }

      /* Reduce the precision. */
      stepSize *= 10;
      if (stepSizePrecision > 0) {
        --stepSizePrecision;
      }
    }

    return {
      stepSize: stepSize,
      stepSizePrecision: stepSizePrecision,
    };
  }

  /**
   * Get the maximun number of equally spaced labels. |fontHeight_| is doubled
   * because the top two labels are both drawn in the same gap.
   * @return {number}
   */
  getMaxNumberOfLabel_() {
    const /** number */ minLabelSpacing =
        2 * this.fontHeight_ + MIN_LABEL_VERTICAL_SPACING;
    let /** number */ maxLabelNum =
        1 + Math.floor(this.height_ / minLabelSpacing);
    if (maxLabelNum < 2) {
      maxLabelNum = 2;
    } else if (maxLabelNum > MAX_VERTICAL_LABEL_NUM) {
      maxLabelNum = MAX_VERTICAL_LABEL_NUM;
    }

    return maxLabelNum;
  }

  /**
   * Get the number of labels with |stepSize|. Because we want the top of the
   * label to be an exact multiple of the |stepSize|, we use |Math.ceil() + 1|
   * to add an additional label above the |maxValue|. See |getTopLabelValue_()|.
   * @param {number} stepSize
   * @return {number}
   */
  getNumberOfLabelWithStepSize_(stepSize) {
    const /** number */ maxValue = this.maxValue_;
    return Math.ceil(maxValue / stepSize) + 1;
  }

  /**
   * Top label value is an exact multiple of |stepSize|.
   * @param {number} maxValue
   * @param {number} stepSize
   * @return {number}
   */
  getTopLabelValue_(maxValue, stepSize) {
    return Math.ceil(maxValue / stepSize) * stepSize;
  }

  /**
   * Transform the value in the current suitable unit to the real value.
   * @param {number} value
   * @return {number}
   */
  getRealValueWithCurrentUnit_(value) {
    return value * Math.pow(this.unitBase_, this.currentUnitIdx_);
  }
}
