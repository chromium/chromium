// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './md_select_demo.css.js';
import {getHtml} from './md_select_demo.html.js';

export interface MdSelectDemoElement {
  $: {
    select: HTMLSelectElement,
  };
}

export class MdSelectDemoElement extends CrLitElement {
  static get is() {
    return 'md-select-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedOption_: {type: String},
    };
  }

  protected accessor selectedOption_: string = 'two';

  protected onSelectValueChanged_() {
    this.selectedOption_ = this.$.select.value;
  }
}

export const tagName = MdSelectDemoElement.is;

customElements.define(MdSelectDemoElement.is, MdSelectDemoElement);
