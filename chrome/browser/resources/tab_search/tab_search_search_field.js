// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/mwb_shared_icons.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {CrSearchFieldBehavior, CrSearchFieldBehaviorInterface} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrSearchFieldBehaviorInterface}
 */
const TabSearchSearchFieldBase =
    mixinBehaviors([ CrSearchFieldBehavior ], PolymerElement);

/** @polymer */
export class TabSearchSearchField extends TabSearchSearchFieldBase {

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

      /** @private {string} */
      shortcut_: {
        type: String,
        value: () => loadTimeData.getString('shortcutText'),
      },
    };
  }

  /** @return {!HTMLInputElement} */
  getSearchInput() {
    return /** @type {!HTMLInputElement} */ (this.$.searchInput);
  }
}

customElements.define(TabSearchSearchField.is, TabSearchSearchField);
