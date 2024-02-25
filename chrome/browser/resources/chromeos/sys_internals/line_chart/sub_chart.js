// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GRID_COLOR, MIN_LABEL_HORIZONTAL_SPACING, SAMPLE_RATE, TEXT_COLOR, UnitLabelAlign, Y_AXIS_TICK_LENGTH} from './constants.js';
import {DataSeries} from './data_series.js';
import {UnitLabel} from './unit_label.js';

/**
 * Create by |LineChart|.
 * Maintains data series which share the same |UnitLabel|, that is,
 * share the same unit set. Also, this object is responsible for drawing the
 * line and the unit label on the line chart.
 * @const
 */
export class SubChart {
  constructor(/** UnitLabel */ label, /** number */ labelAlign) {
    /** @const {UnitLabel} */
    this.label_ = label;

    /** @const {number} */
    this.labelAlign_ = labelAlign;

    /** @type {Array<DataSeries>} */
    this.dataSeriesList_ = [];

    /**
     * The step size between two data points, in millisecond.
     * @type {number}
     */
    this.stepSize_ = 1;

    /**
     * The time to query the data. It will be smaller than the
     * |visibleStartTime|, so the line chart won't leave blanks beside the edge
     * of the chart.
     * @type {number}
     */
    this.queryStartTime_ = 0;

    /**
     * The offset of the current visible range. To make sure we draw the data
     * points at the same absolute position. See also
     * |renderSubCharts_()|.
     * @type {number}
     */
    this.offset_ = 0;

    /** @type {number} */
    this.width_ = 1;

    /** @type {number} */
    this.height_ = 1;

    /**
     * Number of the points need to be draw on the screen.
     * @type {number}
     */
    this.numOfPoint_ = 0;

    /**
     * See |setMaxValue|.
     * @type {number|null}
     */
    this.maxValue_ = null;
  }

  /**
   * Set the layout of this sub chart. Call this function when something changed
   * like the window size, visible range, scale ...etc. See |LineChart|for the
   * parameters' details.
   * @param {number} width
   * @param {number} height
   * @param {number} fontHeight
   * @param {number} visibleStartTime
   * @param {number} scale
   * @param {number} offset - See |renderSubChart_()|.
   */
  setLayout(width, height, fontHeight, visibleStartTime, scale, offset) {
    this.width_ = width;
    this.height_ = height;
    this.offset_ = offset;
    const /** number */ sampleRate = SAMPLE_RATE;

    /* Draw a data point on every |sampleRate| pixels. */
    this.stepSize_ = scale * sampleRate;

    /* First point's position(|queryStartTime|) may go out of the canvas to
     * make the line chart continuous at the begin of the visible range, as well
     * as the last points. */
    this.queryStartTime_ = visibleStartTime - offset * scale;
    const /** number */ queryWidth = width + offset;
    this.numOfPoint_ = Math.ceil(queryWidth / sampleRate) + 1;

    /* Cannot draw the line at the top and the bottom pixel. */
    const labelHeight = height - 2;
    this.label_.setLayout(labelHeight, fontHeight, /* precision */ 2);
    this.updateMaxValue_();
  }

  /**
   * Overwrite the maximum value of this sub chart. If this value is not null,
   * the maximum value of the unit label will be set to this value instead of
   * the real maximum value of data series.
   * @param {number|null} maxValue
   */
  setMaxValue(maxValue) {
    this.maxValue_ = maxValue;
    this.updateMaxValue_();
  }

  /**
   * Calculate the max value for the current layout.
   */
  updateMaxValue_() {
    const /** Array<DataSeries> */ dataSeriesList = this.dataSeriesList_;
    if (this.maxValue_ != null) {
      this.label_.setMaxValue(this.maxValue_);
      return;
    }
    let /** number */ maxValue = 0;
    for (let /** number */ i = 0; i < dataSeriesList.length; ++i) {
      const value = this.getMaxValueFromDataSeries_(dataSeriesList[i]);
      maxValue = Math.max(maxValue, value);
    }
    this.label_.setMaxValue(maxValue);
  }

  /**
   * Query the max value of the query range from the data series.
   * @param {DataSeries} dataSeries
   * @return {number}
   */
  getMaxValueFromDataSeries_(dataSeries) {
    if (!dataSeries.isVisible()) {
      return 0;
    }
    return dataSeries.getMaxValue(
        this.queryStartTime_, this.stepSize_, this.numOfPoint_);
  }

  /**
   * Add a data series to this sub chart.
   * @param {DataSeries} dataSeries
   */
  addDataSeries(dataSeries) {
    this.dataSeriesList_.push(dataSeries);
  }

  /**
   * Get all data series of this sub chart.
   * @return {Array<DataSeries>}
   */
  getDataSeriesList() {
    return this.dataSeriesList_;
  }

  /**
   * Render the lines of all data series.
   * @param {CanvasRenderingContext2D} context
   */
  renderLines(context) {
    const /** Array<DataSeries> */ dataSeriesList = this.dataSeriesList_;
    for (let /** number */ i = 0; i < dataSeriesList.length; ++i) {
      const /** Array<number> */ values =
          this.getValuesFromDataSeries_(dataSeriesList[i]);
      if (!values) {
        continue;
      }
      this.renderLineOfDataSeries_(context, dataSeriesList[i], values);
    }
  }

  /**
   * Query the the data points' values from the data series.
   * @param {DataSeries} dataSeries
   * @return {Array<number>}
   */
  getValuesFromDataSeries_(dataSeries) {
    if (!dataSeries.isVisible()) {
      return [];
    }
    return dataSeries.getValues(
        this.queryStartTime_, this.stepSize_, this.numOfPoint_);
  }

  /**
   * @param {CanvasRenderingContext2D} context
   * @param {DataSeries} dataSeries
   * @param {Array<number>} values
   */
  renderLineOfDataSeries_(context, dataSeries, values) {
    context.strokeStyle = dataSeries.getColor();
    context.fillStyle = dataSeries.getColor();
    context.beginPath();

    const /** number */ sampleRate = SAMPLE_RATE;
    const /** number */ valueScale = this.label_.getScale();
    let /** number */ firstXCoord = this.width_;
    let /** number */ xCoord = -this.offset_;
    for (let /** number */ i = 0; i < values.length; ++i) {
      if (values[i] != null) {
        const /** number */ chartYCoord = Math.round(values[i] * valueScale);
        const /** number */ realYCoord = this.height_ - 1 - chartYCoord;
        context.lineTo(xCoord, realYCoord);
        if (firstXCoord > xCoord) {
          firstXCoord = xCoord;
        }
      }
      xCoord += sampleRate;
    }
    context.stroke();
    this.fillAreaBelowLine_(context, firstXCoord);
  }

  /**
   * @param {CanvasRenderingContext2D} context
   * @param {number} firstXCoord
   */
  fillAreaBelowLine_(context, firstXCoord) {
    context.lineTo(this.width_, this.height_);
    context.lineTo(firstXCoord, this.height_);
    context.globalAlpha = 0.2;
    context.fill();
    context.globalAlpha = 1.0;
  }

  /**
   * @param {CanvasRenderingContext2D} context
   */
  renderUnitLabels(context) {
    const /** Array<string> */ labelTexts = this.label_.getLabels();
    if (labelTexts.length === 0) {
      return;
    }

    let /** number */ tickStartX;
    let /** number */ tickEndX;
    let /** number */ textXCoord;
    if (this.labelAlign_ === UnitLabelAlign.LEFT) {
      context.textAlign = 'left';
      tickStartX = 0;
      tickEndX = Y_AXIS_TICK_LENGTH;
      textXCoord = MIN_LABEL_HORIZONTAL_SPACING;
    } else if (this.labelAlign_ === UnitLabelAlign.RIGHT) {
      context.textAlign = 'right';
      tickStartX = this.width_ - 1;
      tickEndX = this.width_ - 1 - Y_AXIS_TICK_LENGTH;
      textXCoord = this.width_ - MIN_LABEL_HORIZONTAL_SPACING;
    } else {
      console.warn('Unknown label align.');
      return;
    }
    const /** number */ labelYStep = this.height_ / (labelTexts.length - 1);
    this.renderLabelTicks_(
        context, labelTexts, labelYStep, tickStartX, tickEndX);
    this.renderLabelTexts_(context, labelTexts, labelYStep, textXCoord);
  }

  /**
   * Render the tick line for the unit label.
   * @param {CanvasRenderingContext2D} context
   * @param {Array<string>} labelTexts
   * @param {number} labelYStep
   * @param {number} tickStartX
   * @param {number} tickEndX
   */
  renderLabelTicks_(context, labelTexts, labelYStep, tickStartX, tickEndX) {
    context.strokeStyle = GRID_COLOR;
    context.beginPath();
    /* First and last tick are the top and the bottom of the line chart, so
     * don't draw them again. */
    for (let /** number */ i = 1; i < labelTexts.length - 1; ++i) {
      const /** number */ yCoord = labelYStep * i;
      context.moveTo(tickStartX, yCoord);
      context.lineTo(tickEndX, yCoord);
    }
    context.stroke();
  }

  /**
   * Render the texts for the unit label.
   * @param {CanvasRenderingContext2D} context
   * @param {Array<string>} labelTexts
   * @param {number} labelYStep
   * @param {number} textXCoord
   */
  renderLabelTexts_(context, labelTexts, labelYStep, textXCoord) {
    /* The first label cannot align the bottom of the tick or it will go outside
     * the canvas. */
    context.fillStyle = TEXT_COLOR;
    context.textBaseline = 'top';
    context.fillText(labelTexts[0], textXCoord, 0);

    context.textBaseline = 'bottom';
    for (let /** number */ i = 1; i < labelTexts.length; ++i) {
      const /** number */ yCoord = labelYStep * i;
      context.fillText(labelTexts[i], textXCoord, yCoord);
    }
  }

  /**
   * Return true if there is any data series in this sub chart, whatever there
   * are visible or not.
   * @return {boolean}
   */
  shouldRender() {
    return this.dataSeriesList_.length > 0;
  }
}
