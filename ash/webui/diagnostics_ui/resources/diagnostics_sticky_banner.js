// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class DiagnosticsStickyBannerElement extends PolymerElement {
  static get is() {
    return 'diagnostics-sticky-banner';
  }

  static get template() {
    return html`{__html_template__}`
  }

  static get properties() {
    return {
      /** @type {string} */
      bannerMessage: {
        type: String,
        value: '',
      },
    };
  }
};

customElements.define(
    DiagnosticsStickyBannerElement.is, DiagnosticsStickyBannerElement);
