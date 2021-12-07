// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export interface CodeInputElement {
  $: {
    accessCodeInput: CrInputElement;
  }
}

export class CodeInputElement extends PolymerElement {
  static get is() {
    return 'c2c-code-input';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      length: Number,
      value: {
        type: String,
        value: '',
      }
    };
  }

  get crInput() {
    return this.$.accessCodeInput;
  }

  value: string;

  ready() {
    super.ready();
    this.$.accessCodeInput.addEventListener('input', () => {
      this.handleInput();
    });
  }

  clearInput() {
    this.$.accessCodeInput.value = '';
  }

  focusInput() {
    this.$.accessCodeInput.focusInput();
  }

  private handleInput() {
    this.$.accessCodeInput.value = this.$.accessCodeInput.value.toUpperCase();
    this.dispatchEvent(new CustomEvent('access-code-input', {
      detail: {value: this.$.accessCodeInput.value}
    }));
  }
}

customElements.define(CodeInputElement.is, CodeInputElement);