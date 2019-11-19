// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input_style_css.m.js';
import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Coordinate2d} from '../data/coordinate2d.js';
import {CustomMarginsOrientation} from '../data/margins.js';
import {MeasurementSystem} from '../data/measurement_system.js';
import {Size} from '../data/size.js';
import {observerDepsDefined} from '../print_preview_utils.js';

import {InputBehavior} from './input_behavior.js';

/**
 * Radius of the margin control in pixels. Padding of control + 1 for border.
 * @type {number}
 */
const RADIUS_PX = 9;

Polymer({
  is: 'print-preview-margin-control',

  _template: html`{__html_template__}`,

  behaviors: [InputBehavior, I18nBehavior],

  properties: {
    disabled: {
      type: Boolean,
      reflectToAttribute: true,
      observer: 'onDisabledChange_',
    },

    side: {
      type: String,
      reflectToAttribute: true,
    },

    invalid: {
      type: Boolean,
      reflectToAttribute: true,
    },

    invisible: {
      type: Boolean,
      reflectToAttribute: true,
      observer: 'onClipSizeChange_',
    },

    /** @type {?MeasurementSystem} */
    measurementSystem: Object,

    /** @private {boolean} */
    focused_: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
    },

    /** @private {number} */
    positionInPts_: {
      type: Number,
      notify: true,
      value: 0,
    },

    /** @type {number} */
    scaleTransform: {
      type: Number,
      notify: true,
    },

    /** @type {!Coordinate2d} */
    translateTransform: {
      type: Object,
      notify: true,
    },

    /** @type {!Size} */
    pageSize: {
      type: Object,
      notify: true,
    },

    /** @type {?Size} */
    clipSize: {
      type: Object,
      notify: true,
      observer: 'onClipSizeChange_',
    },
  },

  observers:
      ['updatePosition_(positionInPts_, scaleTransform, translateTransform, ' +
       'pageSize, side)'],

  listeners: {
    'input-change': 'onInputChange_',
  },

  /** @return {!HTMLInputElement} The input element for InputBehavior. */
  getInput: function() {
    return /** @type {!HTMLInputElement} */ (this.$.input);
  },

  /**
   * @param {number} valueInPts New value of the margin control's textbox in
   *     pts.
   */
  setTextboxValue: function(valueInPts) {
    const textbox = this.$.input;
    const pts = textbox.value ? this.parseValueToPts_(textbox.value) : null;
    if (!!pts && valueInPts === Math.round(pts)) {
      // If the textbox's value represents the same value in pts as the new one,
      // don't reset. This allows the "undo" command to work as expected, see
      // https://crbug.com/452844.
      return;
    }

    textbox.value = this.serializeValueFromPts_(valueInPts);
  },

  /** @return {number} The current position of the margin control. */
  getPositionInPts: function() {
    return this.positionInPts_;
  },

  /** @param {number} position The new position for the margin control. */
  setPositionInPts: function(position) {
    this.positionInPts_ = position;
  },

  /**
   * @return {string} 'true' or 'false', indicating whether the input should be
   *     aria-hidden.
   * @private
   */
  getAriaHidden_: function() {
    return this.invisible.toString();
  },

  /**
   * Converts a value in pixels to points.
   * @param {number} pixels Pixel value to convert.
   * @return {number} Given value expressed in points.
   */
  convertPixelsToPts: function(pixels) {
    let pts;
    const Orientation = CustomMarginsOrientation;
    if (this.side == Orientation.TOP) {
      pts = pixels - this.translateTransform.y + RADIUS_PX;
      pts /= this.scaleTransform;
    } else if (this.side == Orientation.RIGHT) {
      pts = pixels - this.translateTransform.x + RADIUS_PX;
      pts /= this.scaleTransform;
      pts = this.pageSize.width - pts;
    } else if (this.side == Orientation.BOTTOM) {
      pts = pixels - this.translateTransform.y + RADIUS_PX;
      pts /= this.scaleTransform;
      pts = this.pageSize.height - pts;
    } else {
      assert(this.side == Orientation.LEFT);
      pts = pixels - this.translateTransform.x + RADIUS_PX;
      pts /= this.scaleTransform;
    }
    return pts;
  },

  /**
   * @param {!PointerEvent} event A pointerdown event triggered by this element.
   * @return {boolean} Whether the margin should start being dragged.
   */
  shouldDrag: function(event) {
    return !this.disabled && event.button == 0 &&
        (event.path[0] == this.$.lineContainer || event.path[0] == this.$.line);
  },

  /** @private */
  onDisabledChange_: function() {
    if (this.disabled) {
      this.focused_ = false;
    }
  },

  /**
   * @param {string} value Value to parse to points. E.g. '3.40' or '200'.
   * @return {?number} Value in points represented by the input value.
   * @private
   */
  parseValueToPts_: function(value) {
    value = value.trim();
    if (value.length == 0) {
      return null;
    }
    assert(this.measurementSystem);
    const decimal = this.measurementSystem.decimalDelimiter;
    const thousands = this.measurementSystem.thousandsDelimiter;
    const whole = `(?:0|[1-9]\\d*|[1-9]\\d{0,2}(?:[${thousands}]\\d{3})*)`;
    const fractional = `(?:[${decimal}]\\d*)`;
    const validationRegex =
        new RegExp(`^-?(?:${whole}${fractional}?|${fractional})$`);
    if (validationRegex.test(value)) {
      // Removing thousands delimiters and replacing the decimal delimiter with
      // the dot symbol in order to use parseFloat() properly.
      value = value.replace(new RegExp(`\\${thousands}`, 'g'), '')
                  .replace(decimal, '.');
      return this.measurementSystem.convertToPoints(parseFloat(value));
    }
    return null;
  },

  /**
   * @param {number} value Value in points to serialize.
   * @return {string} String representation of the value in the system's local
   *     units.
   * @private
   */
  serializeValueFromPts_: function(value) {
    assert(this.measurementSystem);
    value = this.measurementSystem.convertFromPoints(value);
    value = this.measurementSystem.roundValue(value);
    // Convert the dot symbol to the decimal delimiter for the locale.
    return value.toString().replace(
        '.', this.measurementSystem.decimalDelimiter);
  },

  /**
   * @param {!CustomEvent<string>} e Contains the new value of the input.
   * @private
   */
  onInputChange_: function(e) {
    if (!e.detail) {
      return;
    }

    const value = this.parseValueToPts_(e.detail);
    if (value === null) {
      this.invalid = true;
      return;
    }

    this.fire('text-change', value);
  },

  /** @private */
  onBlur_: function() {
    this.focused_ = false;
    this.resetAndUpdate();
    this.fire('text-blur', this.invalid || !this.$.input.value);
  },

  /** @private */
  onFocus_: function() {
    this.focused_ = true;
    this.fire('text-focus');
  },

  /** @private */
  updatePosition_: function() {
    if (!observerDepsDefined(Array.from(arguments))) {
      return;
    }

    const Orientation = CustomMarginsOrientation;
    let x = this.translateTransform.x;
    let y = this.translateTransform.y;
    let width = null;
    let height = null;
    if (this.side == Orientation.TOP) {
      y = this.scaleTransform * this.positionInPts_ +
          this.translateTransform.y - RADIUS_PX;
      width = this.scaleTransform * this.pageSize.width;
    } else if (this.side == Orientation.RIGHT) {
      x = this.scaleTransform * (this.pageSize.width - this.positionInPts_) +
          this.translateTransform.x - RADIUS_PX;
      height = this.scaleTransform * this.pageSize.height;
    } else if (this.side == Orientation.BOTTOM) {
      y = this.scaleTransform * (this.pageSize.height - this.positionInPts_) +
          this.translateTransform.y - RADIUS_PX;
      width = this.scaleTransform * this.pageSize.width;
    } else {
      x = this.scaleTransform * this.positionInPts_ +
          this.translateTransform.x - RADIUS_PX;
      height = this.scaleTransform * this.pageSize.height;
    }
    window.requestAnimationFrame(() => {
      this.style.left = Math.round(x) + 'px';
      this.style.top = Math.round(y) + 'px';
      if (width != null) {
        this.style.width = Math.round(width) + 'px';
      }
      if (height != null) {
        this.style.height = Math.round(height) + 'px';
      }
    });
    this.onClipSizeChange_();
  },

  /** @private */
  onClipSizeChange_: function() {
    if (!this.clipSize) {
      return;
    }
    window.requestAnimationFrame(() => {
      const offsetLeft = this.offsetLeft;
      const offsetTop = this.offsetTop;
      this.style.clip = 'rect(' + (-offsetTop) + 'px, ' +
          (this.clipSize.width - offsetLeft) + 'px, ' +
          (this.clipSize.height - offsetTop) + 'px, ' + (-offsetLeft) + 'px)';
    });
  },
});
