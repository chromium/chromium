// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from '../demo.css.js';

import {getHtml} from './cr_checkbox_demo.html.js';

export class CrCheckboxDemoElement extends CrLitElement {
  static get is() {
    return 'cr-checkbox-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      myValue_: {type: Boolean},
    };
  }

  protected accessor myValue_: boolean|undefined;

  protected onCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.myValue_ = e.detail.value;
  }
}

export const tagName = CrCheckboxDemoElement.is;

customElements.define(CrCheckboxDemoElement.is, CrCheckboxDemoElement);
