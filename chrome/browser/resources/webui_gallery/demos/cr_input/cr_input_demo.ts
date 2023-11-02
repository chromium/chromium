// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_hidden_style.css.js';

import {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_input_demo.html.js';

interface CrInputDemoElement {
  $: {
    numberInput: CrInputElement,
  };
}

class CrInputDemoElement extends PolymerElement {
  static get is() {
    return 'cr-input-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      emailValue_: String,
      numberValue_: String,
      pinValue_: String,
      searchValue_: String,
      textValue_: String,
    };
  }

  private emailValue_: string;
  private numberValue_: string;
  private pinValue_: string;
  private searchValue_: string;
  private textValue_: string;

  private onClearSearchClick_() {
    this.searchValue_ = '';
  }

  private onValidateClick_() {
    this.$.numberInput.validate();
  }
}

customElements.define(CrInputDemoElement.is, CrInputDemoElement);
