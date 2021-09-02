// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import './print_preview_shared_css.js';

import {CrSearchFieldBehavior, CrSearchFieldBehaviorInterface} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field_behavior.js';
import {stripDiacritics} from 'chrome://resources/js/search_highlight_utils.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @type {!RegExp} */
const SANITIZE_REGEX = /[-[\]{}()*+?.,\\^$|#\s]/g;


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrSearchFieldBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const PrintPreviewSearchBoxElementBase = mixinBehaviors(
    [CrSearchFieldBehavior, WebUIListenerBehaviorInterface], PolymerElement);

/** @polymer */
export class PrintPreviewSearchBoxElement extends
    PrintPreviewSearchBoxElementBase {
  static get is() {
    return 'print-preview-search-box';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      autofocus: Boolean,

      /** @type {?RegExp} */
      searchQuery: {
        type: Object,
        notify: true,
      },
    };
  }

  constructor() {
    super();

    /**
     * The last search query.
     * @private {string}
     */
    this.lastQuery_ = '';
  }

  /** @override */
  ready() {
    super.ready();

    this.addEventListener(
        'search-changed',
        e => this.onSearchChanged_(/** @type {!CustomEvent<string>} */ (e)));
  }

  /** @return {!CrInputElement} */
  getSearchInput() {
    return /** @type {!CrInputElement} */ (this.$.searchInput);
  }

  focus() {
    this.$.searchInput.focus();
  }

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
  }

  /** @private */
  onClearClick_() {
    this.setValue('');
    this.$.searchInput.focus();
  }
}

customElements.define(
    PrintPreviewSearchBoxElement.is, PrintPreviewSearchBoxElement);
