// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/md_select.css.js';
import '../demo.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './md_select_demo.html.js';

interface MdSelectDemoElement {
  $: {
    select: HTMLSelectElement,
  };
}

class MdSelectDemoElement extends PolymerElement {
  static get is() {
    return 'md-select-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedOption_: String,
    };
  }

  private selectedOption_: string = 'two';

  private onSelectValueChanged_() {
    this.selectedOption_ = this.$.select.value;
  }
}

export const tagName = MdSelectDemoElement.is;

customElements.define(MdSelectDemoElement.is, MdSelectDemoElement);
