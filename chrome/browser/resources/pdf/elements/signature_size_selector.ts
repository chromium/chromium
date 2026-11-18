// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './icons.html.js';

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './signature_size_selector.css.js';
import {getHtml} from './signature_size_selector.html.js';

const MIN_SIGNATURE_SIZE: number = 25;
const MAX_SIGNATURE_SIZE: number = 500;
const DEFAULT_SIGNATURE_SIZE: number = 100;
const SIGNATURE_SIZE_STEP: number = 10;

export interface SignatureSizeSelectorElement {
  $: {
    decrease: CrIconButtonElement,
    increase: CrIconButtonElement,
    percentage: HTMLElement,
  };
}

export class SignatureSizeSelectorElement extends CrLitElement {
  static get is() {
    return 'signature-size-selector';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentSize: {
        notify: true,
        type: Number,
      },
    };
  }

  accessor currentSize: number = DEFAULT_SIGNATURE_SIZE;

  protected isDecreaseDisabled_(): boolean {
    return this.currentSize <= MIN_SIGNATURE_SIZE;
  }

  protected isIncreaseDisabled_(): boolean {
    return this.currentSize >= MAX_SIGNATURE_SIZE;
  }

  protected onDecreaseClick_() {
    const previousSize =
        (Math.ceil(this.currentSize / SIGNATURE_SIZE_STEP) - 1) *
        SIGNATURE_SIZE_STEP;
    this.currentSize = Math.max(MIN_SIGNATURE_SIZE, previousSize);
  }

  protected onIncreaseClick_() {
    const nextSize = (Math.floor(this.currentSize / SIGNATURE_SIZE_STEP) + 1) *
        SIGNATURE_SIZE_STEP;
    this.currentSize = Math.min(MAX_SIGNATURE_SIZE, nextSize);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'signature-size-selector': SignatureSizeSelectorElement;
  }
}

customElements.define(
    SignatureSizeSelectorElement.is, SignatureSizeSelectorElement);
