// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '../demo.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_checkbox_demo.html.js';

class CrCheckboxDemoElement extends PolymerElement {
  static get is() {
    return 'cr-checkbox-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      myValue_: Boolean,
    };
  }

  private myValue_: boolean;
}

export const tagName = CrCheckboxDemoElement.is;

customElements.define(CrCheckboxDemoElement.is, CrCheckboxDemoElement);
