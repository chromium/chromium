// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import './shared_style.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'history-toolbar',

  _template: html`{__html_template__}`,

  properties: {
    // Number of history items currently selected.
    // TODO(calamity): bind this to
    // listContainer.selectedItem.selectedPaths.length.
    count: {
      type: Number,
      value: 0,
      observer: 'changeToolbarView_',
    },

    // True if 1 or more history items are selected. When this value changes
    // the background colour changes.
    itemsSelected_: {
      type: Boolean,
      value: false,
    },

    pendingDelete: Boolean,

    // The most recent term entered in the search field. Updated incrementally
    // as the user types.
    searchTerm: {
      type: String,
      observer: 'searchTermChanged_',
    },

    // True if the backend is processing and a spinner should be shown in the
    // toolbar.
    spinnerActive: {
      type: Boolean,
      value: false,
    },

    hasDrawer: {
      type: Boolean,
      reflectToAttribute: true,
    },

    hasMoreResults: Boolean,

    querying: Boolean,

    queryInfo: Object,

    // Whether to show the menu promo (a tooltip that points at the menu button
    // in narrow mode).
    showMenuPromo: Boolean,
  },

  /** @return {CrToolbarSearchFieldElement} */
  get searchField() {
    return /** @type {CrToolbarElement} */ (this.$['main-toolbar'])
        .getSearchField();
  },

  deleteSelectedItems() {
    this.fire('delete-selected');
  },

  clearSelectedItems() {
    this.fire('unselect-all');
  },

  /**
   * Changes the toolbar background color depending on whether any history items
   * are currently selected.
   * @private
   */
  changeToolbarView_() {
    this.itemsSelected_ = this.count > 0;
  },

  /**
   * When changing the search term externally, update the search field to
   * reflect the new search term.
   * @private
   */
  searchTermChanged_() {
    if (this.searchField.getValue() !== this.searchTerm) {
      this.searchField.showAndFocus();
      this.searchField.setValue(this.searchTerm);
    }
  },

  /**
   * @param {boolean} showMenuPromo
   * @return {boolean}
   * @private
   */
  canShowMenuPromo_(showMenuPromo) {
    return this.showMenuPromo && !loadTimeData.getBoolean('isGuestSession');
  },

  /**
   * @param {!CustomEvent<string>} event
   * @private
   */
  onSearchChanged_(event) {
    this.fire('change-query', {search: event.detail});
  },

  /** @private */
  numberOfItemsSelected_(count) {
    return count > 0 ? loadTimeData.getStringF('itemsSelected', count) : '';
  },
});
