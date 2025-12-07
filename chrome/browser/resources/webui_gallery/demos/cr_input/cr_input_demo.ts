// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_input_demo.css.js';
import {getHtml} from './cr_input_demo.html.js';

export interface CrInputDemoElement {
  $: {
    numberInput: CrInputElement,
  };
}

export class CrInputDemoElement extends CrLitElement {
  static get is() {
    return 'cr-input-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      emailValue_: {type: String},
      numberValue_: {type: String},
      pinValue_: {type: String},
      searchValue_: {type: String},
      textValue_: {type: String},
      textareaValue_: {type: String},
    };
  }

  protected accessor emailValue_: string|undefined;
  protected accessor numberValue_: string|undefined;
  protected accessor pinValue_: string|undefined;
  protected accessor searchValue_: string|undefined;
  protected accessor textValue_: string|undefined;
  protected accessor textareaValue_: string|undefined;

  protected onClearSearchClick_() {
    this.searchValue_ = '';
  }

  protected onValidateClick_() {
    this.$.numberInput.validate();
  }

  protected onTextValueChanged_(e: CustomEvent<{value: string}>) {
    this.textValue_ = e.detail.value;
  }

  protected onSearchValueChanged_(e: CustomEvent<{value: string}>) {
    this.searchValue_ = e.detail.value;
  }

  protected onEmailValueChanged_(e: CustomEvent<{value: string}>) {
    this.emailValue_ = e.detail.value;
  }

  protected onNumberValueChanged_(e: CustomEvent<{value: string}>) {
    this.numberValue_ = e.detail.value;
  }

  protected onPinValueChanged_(e: CustomEvent<{value: string}>) {
    this.pinValue_ = e.detail.value;
  }

  protected onTextareaValueChanged_(e: CustomEvent<{value: string}>) {
    this.textareaValue_ = e.detail.value;
  }
}

export const tagName = CrInputDemoElement.is;

customElements.define(CrInputDemoElement.is, CrInputDemoElement);
