// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './customize_chrome_combobox.html.js';

export interface CustomizeChromeCombobox {
  $: {
    input: HTMLDivElement,
    dropdown: HTMLDivElement,
  };
}

export class CustomizeChromeCombobox extends PolymerElement {
  static get is() {
    return 'customize-chrome-combobox';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      expanded_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      label: String,
    };
  }

  private expanded_: boolean;

  private onInputClick_() {
    this.expanded_ = !this.expanded_;
  }

  private onInputFocusout_() {
    this.expanded_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-combobox': CustomizeChromeCombobox;
  }
}

customElements.define(CustomizeChromeCombobox.is, CustomizeChromeCombobox);
