// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '../demo.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_toggle_demo.html.js';

class CrToggleDemoElement extends PolymerElement {
  static get is() {
    return 'cr-toggle-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      checked_: Boolean,
    };
  }

  private checked_: boolean;
}

export const tagName = CrToggleDemoElement.is;

customElements.define(CrToggleDemoElement.is, CrToggleDemoElement);
