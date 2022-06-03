// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class WhatsNewErrorPageElement extends PolymerElement {
  static get is() {
    return 'whats-new-error-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}
customElements.define(WhatsNewErrorPageElement.is, WhatsNewErrorPageElement);
