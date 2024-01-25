// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input_style.css.js';
import '../strings.m.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Coordinate2d} from '../data/coordinate2d.js';
import {CustomMarginsOrientation} from '../data/margins.js';
import type {MeasurementSystem} from '../data/measurement_system.js';
import type {Size} from '../data/size.js';
import {observerDepsDefined} from '../print_preview_utils.js';

import {InputMixin} from './input_mixin.js';
import {getTemplate} from './margin_control.html.js';

/**
 * Radius of the margin control in pixels. Padding of control + 1 for border.
 */
const RADIUS_PX: number = 9;

export interface PrintPreviewMarginControlElement {
  $: {
    input: HTMLInputElement,
    lineContainer: HTMLDivElement,
    line: HTMLDivElement,
  };
}

const PrintPreviewMarginControlElementBase =
    I18nMixin(WebUiListenerMixin(InputMixin(PolymerElement)));

export class PrintPreviewMarginControlElement extends
    PrintPreviewMarginControlElementBase {
  static get is() {
    return 'print-preview-margin-control';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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

      measurementSystem: Object,

      focused_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      positionInPts_: {
        type: Number,
        notify: true,
        value: 0,
      },

      scaleTransform: {
        type: Number,
        notify: true,
      },

      translateTransform: {
        type: Object,
        notify: true,
      },

      pageSize: {
        type: Object,
        notify: true,
      },

      clipSize: {
        type: Object,
        notify: true,
        observer: 'onClipSizeChange_',
      },
    };
  }

  disabled: boolean;
  side: CustomMarginsOrientation;
  invalid: boolean;
  invisible: boolean;
  measurementSystem: MeasurementSystem|null;
  scaleTransform: number;
  translateTransform: Coordinate2d;
  pageSize: Size;
  clipSize: Size|null;

  private focused_: boolean;
  private positionInPts_: number;

  static get observers() {
    return [
      'updatePosition_(positionInPts_, scaleTransform, translateTransform, ' +
          'pageSize, side)',
    ];
  }

  override ready() {
    super.ready();

    this.addEventListener('input-change', e => this.onInputChange_(e));
  }

  /** @return The input element for InputBehavior. */
  override getInput(): HTMLInputElement {
    return this.$.input;
  }

  /**
   * @param valueInPts New value of the margin control's textbox in pts.
   */
  setTextboxValue(valueInPts: number) {
    const textbox = this.$.input;
    const pts = textbox.value ? this.parseValueToPts_(textbox.value) : null;
    if (pts !== null && valueInPts === Math.round(pts)) {
      // If the textbox's value represents the same value in pts as the new one,
      // don't reset. This allows the "undo" command to work as expected, see
      // https://crbug.com/452844.
      return;
    }

    textbox.value = this.serializeValueFromPts_(valueInPts);
    this.resetString();
  }

  /** @return The current position of the margin control. */
  getPositionInPts(): number {
    return this.positionInPts_;
  }

  /** @param position The new position for the margin control. */
  setPositionInPts(position: number) {
    this.positionInPts_ = position;
  }

  /**
   * @return 'true' or 'false', indicating whether the input should be
   *     aria-hidden.
   */
  private getAriaHidden_(): string {
    return this.invisible.toString();
  }

  /**
   * Converts a value in pixels to points.
   * @param pixels Pixel value to convert.
   * @return Given value expressed in points.
   */
  convertPixelsToPts(pixels: number): number {
    let pts;
    const Orientation = CustomMarginsOrientation;
    if (this.side === Orientation.TOP) {
      pts = pixels - this.translateTransform.y + RADIUS_PX;
      pts /= this.scaleTransform;
    } else if (this.side === Orientation.RIGHT) {
      pts = pixels - this.translateTransform.x + RADIUS_PX;
      pts /= this.scaleTransform;
      pts = this.pageSize.width - pts;
    } else if (this.side === Orientation.BOTTOM) {
      pts = pixels - this.translateTransform.y + RADIUS_PX;
      pts /= this.scaleTransform;
      pts = this.pageSize.height - pts;
    } else {
      assert(this.side === Orientation.LEFT);
      pts = pixels - this.translateTransform.x + RADIUS_PX;
      pts /= this.scaleTransform;
    }
    return pts;
  }

  /**
   * @param event A pointerdown event triggered by this element.
   * @return Whether the margin should start being dragged.
   */
  shouldDrag(event: PointerEvent): boolean {
    return !this.disabled && event.button === 0 &&
        (event.composedPath()[0] === this.$.lineContainer ||
         event.composedPath()[0] === this.$.line);
  }

  private onDisabledChange_() {
    if (this.disabled) {
      this.focused_ = false;
    }
  }

  /**
   * @param value Value to parse to points. E.g. '3.40' or '200'.
   * @return Value in points represented by the input value.
   */
  private parseValueToPts_(value: string): number|null {
    value = value.trim();
    if (value.length === 0) {
      return null;
    }
    assert(this.measurementSystem);
    const decimal = this.measurementSystem!.decimalDelimiter;
    const thousands = this.measurementSystem!.thousandsDelimiter;
    const whole = `(?:0|[1-9]\\d*|[1-9]\\d{0,2}(?:[${thousands}]\\d{3})*)`;
    const fractional = `(?:[${decimal}]\\d+)`;
    const wholeDecimal = `(?:${whole}[${decimal}])`;
    const validationRegex = new RegExp(
        `^-?(?:${whole}${fractional}?|${fractional}|${wholeDecimal})$`);
    if (validationRegex.test(value)) {
      // Removing thousands delimiters and replacing the decimal delimiter with
      // the dot symbol in order to use parseFloat() properly.
      value = value.replace(new RegExp(`\\${thousands}`, 'g'), '')
                  .replace(decimal, '.');
      return this.measurementSystem!.convertToPoints(parseFloat(value));
    }
    return null;
  }

  /**
   * @param value Value in points to serialize.
   * @return String representation of the value in the system's local units.
   */
  private serializeValueFromPts_(value: number): string {
    assert(this.measurementSystem);
    value = this.measurementSystem!.convertFromPoints(value);
    value = this.measurementSystem!.roundValue(value);
    // Convert the dot symbol to the decimal delimiter for the locale.
    return value.toString().replace(
        '.', this.measurementSystem!.decimalDelimiter);
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  /**
   * @param e Contains the new value of the input.
   */
  private onInputChange_(e: CustomEvent<string>) {
    if (e.detail === '') {
      return;
    }

    const value = this.parseValueToPts_(e.detail);
    if (value === null) {
      this.invalid = true;
      return;
    }

    this.fire_('text-change', value);
  }

  private onBlur_() {
    this.focused_ = false;
    this.resetAndUpdate();
    this.fire_('text-blur', this.invalid || !this.$.input.value);
  }

  private onFocus_() {
    this.focused_ = true;
    this.fire_('text-focus');
  }

  private updatePosition_() {
    if (!observerDepsDefined(Array.from(arguments))) {
      return;
    }

    const Orientation = CustomMarginsOrientation;
    let x = this.translateTransform.x;
    let y = this.translateTransform.y;
    let width: number|null = null;
    let height: number|null = null;
    if (this.side === Orientation.TOP) {
      y = this.scaleTransform * this.positionInPts_ +
          this.translateTransform.y - RADIUS_PX;
      width = this.scaleTransform * this.pageSize.width;
    } else if (this.side === Orientation.RIGHT) {
      x = this.scaleTransform * (this.pageSize.width - this.positionInPts_) +
          this.translateTransform.x - RADIUS_PX;
      height = this.scaleTransform * this.pageSize.height;
    } else if (this.side === Orientation.BOTTOM) {
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
      if (width !== null) {
        this.style.width = Math.round(width) + 'px';
      }
      if (height !== null) {
        this.style.height = Math.round(height) + 'px';
      }
    });
    this.onClipSizeChange_();
  }

  private onClipSizeChange_() {
    if (!this.clipSize) {
      return;
    }
    window.requestAnimationFrame(() => {
      const offsetLeft = this.offsetLeft;
      const offsetTop = this.offsetTop;
      this.style.clip = 'rect(' + (-offsetTop) + 'px, ' +
          (this.clipSize!.width - offsetLeft) + 'px, ' +
          (this.clipSize!.height - offsetTop) + 'px, ' + (-offsetLeft) + 'px)';
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-margin-control': PrintPreviewMarginControlElement;
  }
}

customElements.define(
    PrintPreviewMarginControlElement.is, PrintPreviewMarginControlElement);
