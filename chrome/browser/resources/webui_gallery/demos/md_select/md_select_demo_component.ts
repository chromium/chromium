// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './md_select_demo_component.html.js';

interface MdSelectDemoComponent {
  $: {
    select: HTMLSelectElement,
  };
}

class MdSelectDemoComponent extends PolymerElement {
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

customElements.define(MdSelectDemoComponent.is, MdSelectDemoComponent);
