// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import './whats_new_error_page.js';
import './strings.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class WhatsNewAppElement extends PolymerElement {
  static get is() {
    return 'whats-new-app';
  }

  static get properties() {
    return {
      loadFailed_: Boolean,
    };
  }

  // Temporarily default to true while there's no other content.
  private loadFailed_: boolean = true;

  static get template() {
    return html`{__html_template__}`;
  }
}
customElements.define(WhatsNewAppElement.is, WhatsNewAppElement);
