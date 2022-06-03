// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
export class PageToolbarElement extends PolymerElement {
  static get is() {
    return 'page-toolbar';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      title: {
        type: String,
        value: '',
      },

      isNarrow: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  onMenuTap_() {
    this.dispatchEvent(
        new CustomEvent('menu-tap', {bubbles: true, composed: true}));
  }
}

customElements.define(PageToolbarElement.is, PageToolbarElement);
