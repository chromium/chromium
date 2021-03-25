// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import './print_preview_shared_css.js';

import {CrSearchFieldBehavior} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field_behavior.js';
import {stripDiacritics} from 'chrome://resources/js/search_highlight_utils.m.js';
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
  lastQuery_: '',

  /** @return {!CrInputElement} */
  getSearchInput() {
    return /** @type {!CrInputElement} */ (this.$.searchInput);
  },

  focus() {
    this.$.searchInput.focus();
  },

  /**
   * @param {!CustomEvent<string>} e Event containing the new search.
   * @private
   */
  onSearchChanged_(e) {
    const strippedQuery = stripDiacritics(e.detail.trim());
    const safeQuery = strippedQuery.replace(SANITIZE_REGEX, '\\$&');
    if (safeQuery === this.lastQuery_) {
      return;
    }

    this.lastQuery_ = safeQuery;
    this.searchQuery =
        safeQuery.length > 0 ? new RegExp(`(${safeQuery})`, 'ig') : null;
  },

  /** @private */
  onClearClick_() {
    this.setValue('');
    this.$.searchInput.focus();
  },
});
