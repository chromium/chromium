// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import './print_preview_shared_css.js';

import {CrSearchFieldBehavior} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @type {!RegExp} */
const SANITIZE_REGEX = /[-[\]{}()*+?.,\\^$|#\s]/g;

Polymer({
  is: 'print-preview-search-box',

  _template: html`{__html_template__}`,

  behaviors: [CrSearchFieldBehavior],

  properties: {
    autofocus: Boolean,

    /** @type {?RegExp} */
    searchQuery: {
      type: Object,
      notify: true,
    },
  },

  listeners: {
    'search-changed': 'onSearchChanged_',
  },

  /**
   * The last search query.
   * @private {string}
   */
  lastString_: '',

  /** @return {!CrInputElement} */
  getSearchInput: function() {
    return /** @type {!CrInputElement} */ (this.$.searchInput);
  },

  focus: function() {
    this.$.searchInput.focus();
  },

  /**
   * @param {!CustomEvent<string>} e Event containing the new search.
   * @private
   */
  onSearchChanged_: function(e) {
    const safeQueryString = e.detail.trim().replace(SANITIZE_REGEX, '\\$&');
    if (safeQueryString === this.lastString_) {
      return;
    }

    this.lastString_ = safeQueryString;
    this.searchQuery = safeQueryString.length > 0 ?
        new RegExp(`(${safeQueryString})`, 'i') :
        null;
  },

  /** @private */
  onClearClick_: function() {
    this.setValue('');
    this.$.searchInput.focus();
  },
});
