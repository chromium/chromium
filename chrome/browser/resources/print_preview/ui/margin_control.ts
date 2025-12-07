// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {Coordinate2d} from '../data/coordinate2d.js';
import {CustomMarginsOrientation} from '../data/margins.js';
import type {MeasurementSystem} from '../data/measurement_system.js';
import {Size} from '../data/size.js';

import {InputMixin} from './input_mixin.js';
import {getCss} from './margin_control.css.js';
import {getHtml} from './margin_control.html.js';

/**
 * Radius of the margin control in pixels. Padding of control + 1 for border.
 */
const RADIUS_PX: number = 9;

export interface PrintPreviewMarginControlElement {
  $: {
    input: HTMLInputElement,
    lineContainer: HTMLElement,
    line: HTMLElement,
  };
}

const PrintPreviewMarginControlElementBase =
    I18nMixinLit(WebUiListenerMixinLit(InputMixin(CrLitElement)));

export class PrintPreviewMarginControlElement extends
    PrintPreviewMarginControlElementBase {
  static get is() {
    return 'print-preview-margin-control';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {
        type: Boolean,
        reflect: true,
      },

      side: {
        type: String,
        reflect: true,
      },

      invalid: {
        type: Boolean,
        reflect: true,
      },

      invisible: {
        type: Boolean,
        reflect: true,
      },

      measurementSystem: {type: Object},

      focused_: {
        type: Boolean,
        reflect: true,
      },

      positionInPts_: {type: Number},
      scaleTransform: {type: Number},
      translateTransform: {type: Object},
      pageSize: {type: Object},
      clipSize: {type: Object},
    };
  }

  accessor disabled: boolean = false;
  accessor side: CustomMarginsOrientation = CustomMarginsOrientation.TOP;
  accessor invalid: boolean = false;
  accessor invisible: boolean = false;
  accessor measurementSystem: MeasurementSystem|null = null;
  accessor scaleTransform: number = 1;
  accessor translateTransform: Coordinate2d = new Coordinate2d(0, 0);
  accessor pageSize: Size = new Size(612, 792);
  accessor clipSize: Size|null = null;

  private accessor focused_: boolean = false;
  private accessor positionInPts_: number = 0;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('disabled')) {
      if (this.disabled) {
        this.focused_ = false;
      }
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('clipSize') ||
        changedProperties.has('invisible')) {
      this.onClipSizeChange_();
    }

    if (changedPrivateProperties.has('positionInPts_') ||
        changedProperties.has('scaleTransform') ||
        changedProperties.has('translateTransform') ||
        changedProperties.has('pageSize') || changedProperties.has('side')) {
      this.updatePosition_();
    }
  }

  override firstUpdated() {
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
  protected getAriaHidden_(): string {
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
    const decimal = this.measurementSystem.decimalDelimiter;
    const thousands = this.measurementSystem.thousandsDelimiter;
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
      return this.measurementSystem.convertToPoints(parseFloat(value));
    }
    return null;
  }

  /**
   * @param value Value in points to serialize.
   * @return String representation of the value in the system's local units.
   */
  private serializeValueFromPts_(value: number): string {
    assert(this.measurementSystem);
    value = this.measurementSystem.convertFromPoints(value);
    value = this.measurementSystem.roundValue(value);
    // Convert the dot symbol to the decimal delimiter for the locale.
    return value.toString().replace(
        '.', this.measurementSystem.decimalDelimiter);
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

  protected onBlur_() {
    this.focused_ = false;
    this.resetAndUpdate();
    this.fire_('text-blur', this.invalid || !this.$.input.value);
  }

  protected onFocus_() {
    this.focused_ = true;
    this.fire_('text-focus');
  }

  private updatePosition_() {
    if (!this.translateTransform || !this.scaleTransform ||
        !this.measurementSystem) {
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

export type MarginControlElement = PrintPreviewMarginControlElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-margin-control': PrintPreviewMarginControlElement;
  }
}

customElements.define(
    PrintPreviewMarginControlElement.is, PrintPreviewMarginControlElement);
