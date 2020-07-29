// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shared_vars.js';

import {CrSearchFieldBehavior} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field_behavior.m.js';
import {html, PolymerElement, mixinBehaviors} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class TabSearchSearchField extends mixinBehaviors(
  [CrSearchFieldBehavior], PolymerElement) {

  static get is() {
    return 'tab-search-search-field';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Controls autofocus for the search field.
       */
      autofocus: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @return {!HTMLInputElement} */
  getSearchInput() {
    return /** @type {!HTMLInputElement} */ (this.$.searchInput);
  }
}

customElements.define(TabSearchSearchField.is, TabSearchSearchField);
