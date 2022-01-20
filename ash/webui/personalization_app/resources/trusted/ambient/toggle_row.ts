// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays a description text and a toggle button.
 */

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';

import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export interface ToggleRowElement {
  $: {toggle: CrToggleElement}
}

export class ToggleRowElement extends PolymerElement {
  static get is() {
    return 'toggle-row';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      checked: {type: Boolean, value: false, reflectToAttribute: true},
      description: String,
    };
  }
}

customElements.define(ToggleRowElement.is, ToggleRowElement);
