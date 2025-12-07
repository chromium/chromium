// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from '../demo.css.js';

import {getHtml} from './cr_radio_demo.html.js';

export class CrRadioDemoElement extends CrLitElement {
  static get is() {
    return 'cr-radio-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedRadioOption_: {type: String},
    };
  }

  protected accessor selectedRadioOption_: string|undefined;

  protected onSelectedRadioOptionChanged_(e: CustomEvent<{value: string}>) {
    this.selectedRadioOption_ = e.detail.value;
  }
}

export const tagName = CrRadioDemoElement.is;

customElements.define(CrRadioDemoElement.is, CrRadioDemoElement);
