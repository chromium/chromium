// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import './tab_search_item.js';
import './tab_search_search_field.js'

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TabSearchApiProxy, TabSearchApiProxyImpl} from './tab_search_api_proxy.js';

/**
 * @param {string} searchText
 * @param {!tabSearch.mojom.Tab} item
 * @return {boolean}
 */
function filterFunc(searchText, item) {
  return item.title.toLowerCase().includes(searchText.toLowerCase());
};

export class TabSearchAppElement extends PolymerElement {
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

      /**
       * The seleted item's index, or -1 if no item selected.
       * @private {number}
       */
      selectedIndex_: {type: Number, value: -1},

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
    this.apiProxy_ = TabSearchApiProxyImpl.getInstance();

    /** @private {!Array<number>} */
    this.listenerIds_ = [];
  }

  /** @override */
  ready() {
    super.ready();

    this.listenerIds_.push(
        this.apiProxy_.getCallbackRouter().tabsChanged.addListener(
            this.getTabs.bind(this)));
    this.getTabs();
  }

  /** @override */
  disconnectedCallback() {
    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));
  }

  /** @private */
  getTabs() {
    this.apiProxy_.getProfileTabs().then(({profileTabs}) => {
      if (profileTabs) {
        this.openTabs_ = profileTabs.windows;
      }
    });
  }

  /** @return {number} */
  getSelectedIndex() {
    return this.selectedIndex_;
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

    this.selectedIndex_ = result.length > 0 ? 0 : -1;
    return result;
  }

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onSearchChanged_(e) {
    this.searchText_ = e.detail;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemClick_(e) {
    const tabId = Number.parseInt(e.currentTarget.id, 10);
    this.apiProxy_.switchToTab({tabId});
  }

  /**
   * @param {number} index A valid index for an element present in the
   *     filteredOpenTabs_ array.
   * @return {?HTMLElement}
   * @private
   */
  getTabSearchItem_(index) {
    const tabItemId = assert(this.filteredOpenTabs_[index]).tabId;
    return this.shadowRoot.getElementById(tabItemId.toString());
  }

  /**
   * TODO(crbug.com/1111003): Check for the presence of modifiers before
   * triggering a navigation.
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    if (this.selectedIndex_ !== -1) {
      switch (e.key) {
        case 'ArrowUp':
          this.selectItem_(-1);
          break;
        case 'ArrowDown':
          this.selectItem_(1);
          break;
        case 'Home':
          this.selectItem_(-this.selectedIndex_);
          break;
        case 'End':
          this.selectItem_(
              this.filteredOpenTabs_.length - 1 - this.selectedIndex_);
          break;
        case 'Enter':
          const selectedItem = this.filteredOpenTabs_[this.selectedIndex_];
          this.apiProxy_.switchToTab({tabId: selectedItem.tabId});
          break;
      }
    }

    e.stopPropagation();
  }

  /**
   * @param {number} offset Distance from the desired item to select and the
   *     currently selected item.
   * @private
   */
  selectItem_(offset) {
    const length = assert(this.filteredOpenTabs_.length);
    this.selectedIndex_ = (this.selectedIndex_ + offset + length) % length;

    // Ensure the scroll view can fully display a preceding or following tab
    // item if existing. Use Math.sign to identify any such preceding or
    // following item.
    const scrollToIndex =
        (this.selectedIndex_ === 0 ||
         this.selectedIndex_ === this.filteredOpenTabs_.length - 1) ?
        this.selectedIndex_ :
        this.selectedIndex_ + Math.sign(offset);

    this.getTabSearchItem_(scrollToIndex)
        .scrollIntoView({behavior: 'smooth', block: 'nearest'});
  }
}

customElements.define(TabSearchAppElement.is, TabSearchAppElement);
