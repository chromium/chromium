// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './modules.html.js';

/** Container for the NTP modules. */
export class ModulesV2Element extends PolymerElement {
  static get is() {
    return 'ntp-modules-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }
}

customElements.define(ModulesV2Element.is, ModulesV2Element);
