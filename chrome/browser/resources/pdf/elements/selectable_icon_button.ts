// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrRadioButtonMixinLit} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './selectable_icon_button.css.js';
import {getHtml} from './selectable_icon_button.html.js';

export interface SelectableIconButtonElement {
  $: {
    button: CrIconButtonElement,
  };
}

const SelectableIconButtonElementBase = CrRadioButtonMixinLit(CrLitElement);

export class SelectableIconButtonElement extends
    SelectableIconButtonElementBase {
  static get is() {
    return 'selectable-icon-button';
  }

  static override get properties() {
    return {
      icon: {type: String},
    };
  }

  accessor icon: string = '';
  override noRipple: boolean = true;

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'selectable-icon-button': SelectableIconButtonElement;
  }
}

customElements.define(
    SelectableIconButtonElement.is, SelectableIconButtonElement);
