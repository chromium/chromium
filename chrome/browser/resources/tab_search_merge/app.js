// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/mwb_shared_style.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import './tab_search_item.js';
import './tab_search_search_field.js'

import {assert} from 'chrome://resources/js/assert.m.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {listenOnce} from 'chrome://resources/js/util.m.js';
import {TabSearchApiProxy, TabSearchApiProxyImpl} from './tab_search_api_proxy.js';

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
      openTabs_: {
        type: Array,
        observer: 'openTabsChanged_',
      },

      /** @private {!Array<!tabSearch.mojom.Tab>} */
      filteredOpenTabs_: {
        type: Array,
        value: [],
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

    // TODO(tluk): The listener should provide the data needed to update the
    // WebUI without having to make another round trip request to the Browser.
    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.tabsChanged.addListener(() => this.updateTabs_()),
        callbackRouter.tabUpdated.addListener(tab => this.onTabUpdated_(tab)));
    this.updateTabs_();
  }

  /** @override */
  disconnectedCallback() {
    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));
  }

  /** @private */
  updateTabs_() {
    this.apiProxy_.getProfileTabs().then(({profileTabs}) => {
      // Prior to the first load |this.openTabs_| has not been set. Record the
      // time it takes for the initial list of tabs to render.
      if (!this.openTabs_) {
        listenOnce(this.$.tabsList, 'rendered-item-count-changed', e => {
          const event = /** @type {!CustomEvent<!{value: number}>} */ (e);
          // Ensure that the full list of tabs has been rendered.
          assert(event.detail.value === this.filteredOpenTabs_.length);

          // Push showUI() to the event loop to allow reflow to occur following
          // the DOM update.
          setTimeout(() => {
            this.apiProxy_.showUI();
            chrome.metricsPrivate.recordTime(
              'Tabs.TabSearch.WebUI.InitialTabsRenderTime',
              Math.round(window.performance.now()));
          }, 0);
        });
      }
      this.openTabs_ = profileTabs.windows;
    });
  }

  /**
   * @param {!tabSearch.mojom.Tab} updatedTab
   * @private
   */
  onTabUpdated_(updatedTab) {
    const updatedTabId = updatedTab.tabId;
    const windows = this.openTabs_;
    if (windows) {
      for (const window of windows) {
        const {tabs} = window;
        for (let i = 0; i < tabs.length; ++i) {
          // Replace the tab with the same tabId and trigger rerender.
          if (tabs[i].tabId === updatedTabId) {
            tabs[i] = updatedTab;
            this.openTabs_ = windows.concat();
            return;
          }
        }
      }
    }
  }

  /** @return {number} */
  getSelectedIndex() {
    return this.selectedIndex_;
  }

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onSearchChanged_(e) {
    this.searchText_ = e.detail;

    this.updateFilteredTabs_(this.openTabs_ || []);
    // Reset the selected item whenever a search query is provided.
    this.selectedIndex_ = this.filteredOpenTabs_.length > 0 ? 0 : -1;
    this.$.tabs.scrollTop = 0;
  }

  /** @private */
  onFeedbackClick_() {
    this.apiProxy_.showFeedbackPage();
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemClick_(e) {
    const tabId = Number.parseInt(e.currentTarget.id, 10);
    this.apiProxy_.switchToTab({tabId}, !!this.searchText_);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemClose_(e) {
    const tabId = Number.parseInt(e.currentTarget.id, 10);
    this.apiProxy_.closeTab(tabId);
  }

  /**
   * @param {!Array<!tabSearch.mojom.WindowTabs>} newOpenTabs
   * @private
   */
  openTabsChanged_(newOpenTabs) {
    this.updateFilteredTabs_(newOpenTabs);

    // If there was no previously selected index, set the first item as
    // selected; else retain the currently selected index. If the list
    // shrunk above the selected index, select the last index in the list.
    // If there are no matching results, set the selected index value to none.
    this.selectedIndex_ = Math.min(
        Math.max(this.selectedIndex_, 0), this.filteredOpenTabs_.length - 1);
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
   * TODO(crbug.com/1113470): Tab Search item and buttons focus and navigation.
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    // Do not interfere with the search field's management of text selection.
    if (e.shiftKey) {
      return;
    }

    e.stopPropagation();

    if (this.selectedIndex_ === -1) {
      // No tabs matching the search text criteria.
      return;
    }

    switch (e.key) {
      case 'ArrowUp':
        this.selectItem_(-1);
        e.preventDefault();
        break;
      case 'ArrowDown':
        this.selectItem_(1);
        e.preventDefault();
        break;
      case 'Home':
        this.selectItem_(-this.selectedIndex_);
        e.preventDefault();
        break;
      case 'End':
        this.selectItem_(
            this.filteredOpenTabs_.length - 1 - this.selectedIndex_);
        e.preventDefault();
        break;
      case 'Enter':
        const selectedItem = this.filteredOpenTabs_[this.selectedIndex_];
        this.apiProxy_.switchToTab({tabId : selectedItem.tabId},
                                   !!this.searchText_);
        break;
    }
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
    if (this.selectedIndex_ === 0 ||
        this.selectedIndex_ === this.filteredOpenTabs_.length - 1) {
      this.getTabSearchItem_(this.selectedIndex_).scrollIntoView({
        behavior: 'smooth'
      });
    } else {
      this.getTabSearchItem_(this.selectedIndex_ + Math.sign(offset))
          .scrollIntoView(
              {behavior: 'smooth', block: offset > 0 ? 'end' : 'start'});
    }
  }

  /**
   * @return {string}
   * @private
   */
  getKeyboardShortcut_() {
    return (isMac ? 'Cmd' : 'Ctrl') + '+Shift+E';
  }

  /**
   * @param {!Array<!tabSearch.mojom.WindowTabs>} windowTabs
   * @private
   */
  updateFilteredTabs_(windowTabs) {
    const lowerCaseSearchText = this.searchText_.toLowerCase();
    this.filteredOpenTabs_ = !!this.searchText_ ?
        windowTabs
            .map(window => {
              return window.tabs.filter(item => {
                return item.title.toLowerCase().includes(lowerCaseSearchText);
              });
            })
            .flat() :
        windowTabs.map(window => window.tabs).flat();
  }
}

customElements.define(TabSearchAppElement.is, TabSearchAppElement);
