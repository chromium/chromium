// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-search-selection-dialog' is a dialog for setting
 * the preferred search engine.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/md_select_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '../../settings_shared_css.js';

import {addWebUIListener, removeWebUIListener, sendWithPromise, WebUIListener} from '//resources/js/cr.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
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

  /** @private {?SearchEnginesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = SearchEnginesBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    const updateSearchEngines = searchEngines => {
      this.set('searchEngines_', searchEngines.defaults);
    };
    this.browserProxy_.getSearchEnginesList().then(updateSearchEngines);
    addWebUIListener('search-engines-changed', updateSearchEngines);
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
