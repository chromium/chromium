// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TabSearchApiProxy} from './tab_search_api_proxy.js';

/**
 * @param {string} searchText
 * @param {!tabSearch.mojom.Tab} item
 * @return {boolean}
 */
function filterFunc(searchText, item) {
  return item.title.toLowerCase().includes(searchText.toLowerCase());
};

class TabSearchElement extends PolymerElement {
  static get is() {
    return 'tab-search-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {string} */
      searchText_: {
        type: String,
        value: '',
      },

      /** @private {?Array<!tabSearch.mojom.WindowTabs>} */
      openTabs_: Array,

      /** @private {!Array<!tabSearch.mojom.Tab>} */
      filteredOpenTabs_: {
        type: Array,
        computed: 'getFilteredTabs_(openTabs_, searchText_)',
      },
    };
  }

  constructor() {
    super();
    /** @private {!TabSearchApiProxy} */
    this.apiProxy_ = TabSearchApiProxy.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    this.apiProxy_.getProfileTabs().then(({profileTabs}) => {
      if (profileTabs) {
        this.openTabs_ = profileTabs.windows;
      }
    });
  }

  /**
   * @param {!Event} e
   * @private
   */
  onSearchInput_(e) {
    this.searchText_ = e.target.value;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemClick_(e) {
    const tabId = parseInt(e.currentTarget.id, 10);
    this.apiProxy_.switchToTab({tabId});
  }

  /**
   * @param {?Array<!tabSearch.mojom.WindowTabs>} windows
   * @param {string} searchText
   * @return {!Array<!tabSearch.mojom.Tab>}
   * @private
   */
  getFilteredTabs_(windows, searchText) {
    const result = [];
    if (windows) {
      windows.forEach(window => {
        result.push(...window.tabs.filter(filterFunc.bind(null, searchText)));
      });
    }
    return result;
  }
}

customElements.define(TabSearchElement.is, TabSearchElement);
