// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import type {CrSliderElement, SliderTick} from '//resources/cr_elements/cr_slider/cr_slider.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {PageHandlerFactory, PageHandlerRemote} from './watermark.mojom-webui.js';

export interface WatermarkAppElement {
  $: {
    fontSizeInput: CrInputElement,
    fontSizeInputError: HTMLDivElement,
    fillOpacitySlider: CrSliderElement,
    outlineOpacitySlider: CrSliderElement,
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
    };
  }

  protected accessor fontSize_: number = 24;
  protected accessor fillOpacity_: number = 4;
  protected accessor outlineOpacity_: number = 6;
  protected accessor opacityTicks_: SliderTick[] = [];
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
      this.sendStyleToBackend_();
    }
  }

  override firstUpdated() {
    this.sendStyleToBackend_();
  }

  protected sendStyleToBackend_() {
    this.pageHandler_.setWatermarkStyle({
      fillOpacity: this.fillOpacity_,
      outlineOpacity: this.outlineOpacity_,
      fontSize: this.fontSize_,
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
  }

  protected onFontSizeChanged_() {
    const parsedValue = parseInt(this.$.fontSizeInput.value, 10);
    if (isNaN(parsedValue) || parsedValue < 1 || parsedValue > 500) {
      this.$.fontSizeInputError.style.visibility = 'visible';
      return;
    }
    this.$.fontSizeInputError.style.visibility = 'hidden';
    this.fontSize_ = parsedValue;
    this.sendStyleToBackend_();
  }

  // To prevent special floating point chars such as e or '.'
  protected onFontSizeInputKeyDown_(event: KeyboardEvent) {
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

  protected onFillOpacityChanged_() {
    this.fillOpacity_ = Math.round(this.$.fillOpacitySlider.value);
    this.sendStyleToBackend_();
  }

  protected onOutlineOpacityChanged_() {
    this.outlineOpacity_ = Math.round(this.$.outlineOpacitySlider.value);
    this.sendStyleToBackend_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'watermark-app': WatermarkAppElement;
  }
}

customElements.define(WatermarkAppElement.is, WatermarkAppElement);
