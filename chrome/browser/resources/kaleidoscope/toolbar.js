// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_toolbar/cr_toolbar.m.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class KaleidoscopeToolbarElement extends PolymerElement {
  static get is() {
    return 'kaleidoscope-toolbar';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      // Controls whether the search field is shown.
      showSearch: {type: Boolean, value: false},

      // Sets the tooltip text displayed on the menu button.
      menuLabel: {type: String, value: ''},

      // Sets the text displayed beside the menu button.
      pageName: {type: String, value: ''},
    };
  }
}

customElements.define(
    KaleidoscopeToolbarElement.is, KaleidoscopeToolbarElement);
