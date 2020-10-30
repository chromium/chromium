// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-search-selection-dialog' is a dialog for setting
 * the preferred search engine.
 */
Polymer({
  is: 'os-settings-search-selection-dialog',

  behaviors: [],

  properties: {
    /**
     * List of default search engines available.
     * @private {!Array<!SearchEngine>}
     */
    searchEngines_: {
      type: Array,
      value() {
        return [];
      }
    },
  },

  /** @private {?settings.SearchEnginesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.SearchEnginesBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    const updateSearchEngines = searchEngines => {
      this.set('searchEngines_', searchEngines.defaults);
    };
    this.browserProxy_.getSearchEnginesList().then(updateSearchEngines);
    cr.addWebUIListener('search-engines-changed', updateSearchEngines);
  },

  /**
   * Enables the checked languages.
   * @private
   */
  onActionButtonClick_() {
    const select = /** @type {!HTMLSelectElement} */ (this.$$('select'));
    const searchEngine = this.searchEngines_[select.selectedIndex];
    this.browserProxy_.setDefaultSearchEngine(searchEngine.modelIndex);

    this.$.dialog.close();
  },

  /** @private */
  onCancelButtonClick_() {
    this.$.dialog.close();
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeydown_(e) {
    if (e.key === 'Escape') {
      this.onCancelButtonClick_();
    }
  },
});
