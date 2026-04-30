// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import type {CrSliderElement, SliderTick} from '//resources/cr_elements/cr_slider/cr_slider.js';
import type {CrTextareaElement} from '//resources/cr_elements/cr_textarea/cr_textarea.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {PageHandlerFactory, PageHandlerRemote} from './watermark.mojom-webui.js';

export const FONT_SIZE_MIN = 1;
export const FONT_SIZE_MAX = 500;

export interface WatermarkAppElement {
  $: {
    fontSizeInput: CrInputElement,
    fontSizeInputError: HTMLDivElement,
    fillOpacitySlider: CrSliderElement,
    outlineOpacitySlider: CrSliderElement,
    watermarkTextInput: CrTextareaElement,
  };
}

export class WatermarkAppElement extends CrLitElement {
  static get is() {
    return 'watermark-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      fontSize_: {type: Number},
      fillOpacity_: {type: Number},
      outlineOpacity_: {type: Number},
      opacityTicks_: {type: Array},
      watermarkText_: {type: String},
    };
  }

  protected accessor fontSize_: number = 24;
  protected accessor fillOpacity_: number = 4;
  protected accessor outlineOpacity_: number = 6;
  protected accessor opacityTicks_: SliderTick[] = [];
  protected accessor watermarkText_: string = 'Watermark Test Page';
  private pageHandler_: PageHandlerRemote;

  constructor() {
    super();
    this.pageHandler_ = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.pageHandler_.$.bindNewPipeAndPassReceiver());

    for (let i = 0; i <= 100; i++) {
      this.opacityTicks_.push({label: String(i), value: i});
    }

    document.addEventListener(
        'visibilitychange', this.handleVisibilityChange_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener(
        'visibilitychange', this.handleVisibilityChange_.bind(this));
  }

  /**
   * Called when the tab becomes visible. This ensures the watermark is
   * correctly styled if another tab was affecting its state.
   */
  private handleVisibilityChange_() {
    if (document.visibilityState === 'visible') {
      this.sendWatermarkToBackend_();
    }
  }

  override firstUpdated() {
    this.$.fontSizeInput.value = this.fontSize_.toString();
    this.sendWatermarkToBackend_();
  }

  protected sendWatermarkToBackend_() {
    this.pageHandler_.setWatermarkSettings({
      fillOpacity: this.fillOpacity_,
      outlineOpacity: this.outlineOpacity_,
      fontSize: this.fontSize_,
      watermarkText: this.watermarkText_,
    });
  }

  protected onCopyJsonClick_() {
    navigator.clipboard.writeText(JSON.stringify(
        {
          WatermarkStyle: {
            fill_opacity: this.fillOpacity_,
            outline_opacity: this.outlineOpacity_,
            font_size: this.fontSize_,
          },
        },
        null, 2));
    this.pageHandler_.showNotificationToast();
  }

  protected onFontSizeInputMousedown_(event: Event) {
    // To prevent focus loss on the input when the button is clicked.
    event.preventDefault();
  }

  protected updateFontSizeValue_(currentValue: number, newValue: number) {
    if (currentValue === newValue) {
      return;
    }
    this.fontSize_ = newValue;
    this.$.fontSizeInput.value = newValue.toString();
    this.sendWatermarkToBackend_();
  }

  protected onIncrementFontSizeClick_(_event: Event) {
    this.$.fontSizeInput.focus();

    const parsedValue = parseInt(this.$.fontSizeInput.value, 10);
    const newValue = Math.max(FONT_SIZE_MIN, parsedValue + 1);
    this.updateFontSizeValue_(parsedValue, newValue);
  }

  protected onDecrementFontSizeClick_(_event: Event) {
    this.$.fontSizeInput.focus();

    const parsedValue = parseInt(this.$.fontSizeInput.value, 10);
    const newValue = Math.min(FONT_SIZE_MAX, parsedValue - 1);
    this.updateFontSizeValue_(parsedValue, newValue);
  }

  protected onFontSizeValueChanged_() {
    const parsedValue = parseInt(this.$.fontSizeInput.value, 10);

    if (isNaN(parsedValue)) {
      this.$.fontSizeInputError.style.visibility = 'visible';
      return;
    } else if (parsedValue < FONT_SIZE_MIN || parsedValue > FONT_SIZE_MAX) {
      this.$.fontSizeInputError.style.visibility = 'visible';
    } else {
      this.$.fontSizeInputError.style.visibility = 'hidden';
    }

    // Mapping value to range (FONT_SIZE_MIN, FONT_SIZE_MAX)
    const valueWithinRange =
        Math.min(Math.max(parsedValue, FONT_SIZE_MIN), FONT_SIZE_MAX);

    // Ensures that font size is the closest to the enterd value
    if (this.fontSize_ !== valueWithinRange) {
      this.fontSize_ = valueWithinRange;
      this.sendWatermarkToBackend_();
    }
  }

  protected onWatermarkTextValueChanged_() {
    this.watermarkText_ = this.$.watermarkTextInput.value;
    this.sendWatermarkToBackend_();
  }

  // To prevent special floating point chars such as e or '.'
  protected onFontSizeInputKeydown_(event: KeyboardEvent) {
    const allowedNumericKeys =
        ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9'];
    const allowedControlKeys = [
      'Backspace', 'Delete', 'ArrowLeft', 'ArrowRight', 'Tab',
      'v',  // Allows for paste operations.
    ];

    // Don't interrupt on control keys.
    if (allowedControlKeys.includes(event.key) || event.ctrlKey ||
        event.metaKey) {
      return;
    }
    if (!allowedNumericKeys.includes(event.key) ||
      this.$.fontSizeInput.value.length === 3) {
      event.preventDefault();
    }
  }

  protected onFillOpacityCrSliderValueChanged_() {
    this.fillOpacity_ = Math.round(this.$.fillOpacitySlider.value);
    this.sendWatermarkToBackend_();
  }

  protected onOutlineOpacityCrSliderValueChanged_() {
    this.outlineOpacity_ = Math.round(this.$.outlineOpacitySlider.value);
    this.sendWatermarkToBackend_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'watermark-app': WatermarkAppElement;
  }
}

customElements.define(WatermarkAppElement.is, WatermarkAppElement);
