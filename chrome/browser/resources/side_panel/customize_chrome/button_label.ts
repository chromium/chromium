// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './button_label.css.js';
import {getHtml} from './button_label.html.js';

export interface ButtonLabelElement {
  $: {
    label: HTMLButtonElement,
    labelDescription: HTMLElement,
  };
}

export class ButtonLabelElement extends CrLitElement {
  static get is() {
    return 'customize-chrome-button-label';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      label: {type: String},
      labelDescription: {type: String},
    };
  }

  label: string = '';
  labelDescription: string|null = null;
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-button-label': ButtonLabelElement;
  }
}

customElements.define(ButtonLabelElement.is, ButtonLabelElement);
