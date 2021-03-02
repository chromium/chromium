// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/mwb_shared_style.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import './infinite_list.js';
import './tab_search_item.js';
import './tab_search_search_field.js';
import './strings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {listenOnce} from 'chrome://resources/js/util.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {fuzzySearch} from './fuzzy_search.js';
import {InfiniteList, NO_SELECTION, selectorNavigationKeys} from './infinite_list.js';
import {TabData} from './tab_data.js';
import {Tab, Window} from './tab_search.mojom-webui.js';
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

      /** @private {!Array<!TabData>}*/
      openTabs_: {
        type: Array,
        value: [],
      },

      /** @private {!Array<!TabData>} */
      filteredOpenTabs_: {
        type: Array,
        value: [],
      },

      /**
       * Options for fuzzy search.
       * @private {!Object}
       */
      fuzzySearchOptions_: {
        type: Object,
        value: {
          includeScore: true,
          includeMatches: true,
          ignoreLocation: true,
          threshold: 0.0,
          distance: 200,
          keys: [
            {
              name: 'tab.title',
              weight: 2,
            },
            {
              name: 'hostname',
              weight: 1,
            }
          ],
        },
      },

      /** @private {boolean} */
      feedbackButtonEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('submitFeedbackEnabled'),
      },

      /** @private {boolean} */
      moveActiveTabToBottom_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('moveActiveTabToBottom'),
      },

      /** @private */
      searchResultText_: {type: String, value: ''}
    };
  }

  constructor() {
    super();

    /** @private {!TabSearchApiProxy} */
    this.apiProxy_ = TabSearchApiProxyImpl.getInstance();

    /** @private {!Array<number>} */
    this.listenerIds_ = [];

    /** @private {!Function} */
    this.visibilityChangedListener_ = () => {
      // Refresh Tab Search's tab data when transitioning into a visible state.
      if (document.visibilityState === 'visible') {
        this.updateTabs_();
      } else {
        this.onDocumentHidden_();
      }
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.addEventListener('keydown', (e) => {
      this.onKeyDown_(/** @type {!KeyboardEvent} */ (e));
    });

    // Update option values for fuzzy search from feature params.
    this.fuzzySearchOptions_ = Object.assign({}, this.fuzzySearchOptions_, {
      ignoreLocation: loadTimeData.getBoolean('searchIgnoreLocation'),
      threshold: loadTimeData.getValue('searchThreshold'),
      distance: loadTimeData.getInteger('searchDistance'),
      keys: [
        {
          name: 'tab.title',
          weight: loadTimeData.getValue('searchTitleToHostnameWeightRatio'),
        },
        {
          name: 'hostname',
          weight: 1,
        }
      ],
    });
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    document.addEventListener(
        'visibilitychange', this.visibilityChangedListener_);

    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.tabsChanged.addListener(
            profileData => this.openTabsChanged_(profileData.windows)),
        callbackRouter.tabUpdated.addListener(tab => this.onTabUpdated_(tab)),
        callbackRouter.tabsRemoved.addListener(
            tabIds => this.onTabsRemoved_(tabIds)));

    // If added in a visible state update current tabs.
    if (document.visibilityState === 'visible') {
      this.updateTabs_();
    }
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));

    document.removeEventListener(
        'visibilitychange', this.visibilityChangedListener_);
  }

  /** @private */
  onDocumentHidden_() {
    (this.$.tabsList).selected = NO_SELECTION;
    this.$.searchField.setValue('');
    this.$.searchField.getSearchInput().focus();
  }

  /** @private */
  updateTabs_() {
    const getTabsStartTimestamp = Date.now();
    this.apiProxy_.getProfileData().then(({profileData}) => {
      chrome.metricsPrivate.recordTime(
          'Tabs.TabSearch.WebUI.TabListDataReceived',
          Math.round(Date.now() - getTabsStartTimestamp));

      // The infinite-list only triggers a dom-change event after it is ready
      // and observes a change on the list items.
      listenOnce(this.$.tabsList, 'dom-change', () => {
        // Push showUI() to the event loop to allow reflow to occur following
        // the DOM update.
        setTimeout(() => this.apiProxy_.showUI(), 0);
      });

      this.openTabsChanged_(profileData.windows);
    });
  }

  /**
   * @param {!Tab} updatedTab
   * @private
   */
  onTabUpdated_(updatedTab) {
    // Replace the tab with the same tabId and trigger rerender.
    for (let i = 0; i < this.openTabs_.length; ++i) {
      if (this.openTabs_[i].tab.tabId === updatedTab.tabId) {
        this.openTabs_[i] =
            this.tabData_(updatedTab, this.openTabs_[i].inActiveWindow);
        this.updateFilteredTabs_(this.openTabs_);
        return;
      }
    }
  }

  /**
   * @param {!Array<number>} tabIds
   * @private
   */
  onTabsRemoved_(tabIds) {
    if (this.openTabs_.length === 0) {
      return;
    }

    const ids = new Set(tabIds);
    // Splicing in descending index order to avoid affecting preceding indices
    // that are to be removed.
    for (let i = this.openTabs_.length - 1; i >= 0; i--) {
      if (ids.has(this.openTabs_[i].tab.tabId)) {
        this.openTabs_.splice(i, 1);
      }
    }

    this.filteredOpenTabs_ =
        this.filteredOpenTabs_.filter(tabData => !ids.has(tabData.tab.tabId));
  }

  /**
   * The seleted item's index, or -1 if no item selected.
   * @return {number}
   */
  getSelectedIndex() {
    return /** @type {!InfiniteList} */ (this.$.tabsList).selected;
  }

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onSearchChanged_(e) {
    this.searchText_ = e.detail;

    this.updateFilteredTabs_(this.openTabs_);
    // Reset the selected item whenever a search query is provided.
    /** @type {!InfiniteList} */ (this.$.tabsList).selected =
        this.filteredOpenTabs_.length > 0 ? 0 : NO_SELECTION;

    this.$.searchField.announce(this.getA11ySearchResultText_());
  }

  /**
   * @return {string}
   * @private
   */
  getA11ySearchResultText_() {
    const length = this.filteredOpenTabs_.length;
    let text;
    if (this.searchText_.length > 0) {
      text = loadTimeData.getStringF(
          length == 1 ? 'a11yFoundTabFor' : 'a11yFoundTabsFor', length,
          this.searchText_);
    } else {
      text = loadTimeData.getStringF(
          length == 1 ? 'a11yFoundTab' : 'a11yFoundTabs', length);
    }
    return text;
  }

  /** @private */
  onFeedbackClick_() {
    this.apiProxy_.showFeedbackPage();
  }

  /** @private */
  onFeedbackFocus_() {
    /** @type {!InfiniteList} */ (this.$.tabsList).selected = NO_SELECTION;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemClick_(e) {
    const tabId = Number.parseInt(e.currentTarget.id, 10);
    this.apiProxy_.switchToTab(
        {tabId}, !!this.searchText_, /** @type {number} */ (e.model.index));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemClose_(e) {
    performance.mark('close_tab:benchmark_begin');
    const tabId = Number.parseInt(e.currentTarget.id, 10);
    this.apiProxy_.closeTab(
        tabId, !!this.searchText_, /** @type {number} */ (e.model.index));
    this.announceA11y_(loadTimeData.getString('a11yTabClosed'));
    listenOnce(this.$.tabsList, 'iron-items-changed', () => {
      performance.mark('close_tab:benchmark_end');
    });
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemKeyDown_(e) {
    if (e.key !== 'Enter' && e.key !== ' ') {
      return;
    }

    e.stopPropagation();
    e.preventDefault();

    this.apiProxy_.switchToTab(
        {tabId: this.getSelectedTab_().tabId}, !!this.searchText_,
        this.getSelectedIndex());
  }

  /**
   * @param {!Array<!Window>} newOpenWindows
   * @private
   */
  openTabsChanged_(newOpenWindows) {
    this.openTabs_ = [];
    newOpenWindows.forEach(({active, tabs}) => {
      tabs.forEach(tab => {
        this.openTabs_.push(this.tabData_(tab, active));
      });
    });
    this.updateFilteredTabs_(this.openTabs_);

    // If there was no previously selected index, set the first item as
    // selected; else retain the currently selected index. If the list
    // shrunk above the selected index, select the last index in the list.
    // If there are no matching results, set the selected index value to none.
    const tabsList = /** @type {!InfiniteList} */ (this.$.tabsList);
    tabsList.selected = Math.min(
        Math.max(this.getSelectedIndex(), 0),
        this.filteredOpenTabs_.length - 1);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemFocus_(e) {
    // Ensure that when a TabSearchItem receives focus, it becomes the selected
    // item in the list.
    /** @type {!InfiniteList} */ (this.$.tabsList).selected =
        /** @type {number} */ (e.model.index);
  }

  /** @private */
  onSearchFocus_() {
    const tabsList = /** @type {!InfiniteList} */ (this.$.tabsList);
    if (tabsList.selected === NO_SELECTION &&
        this.filteredOpenTabs_.length > 0) {
      tabsList.selected = 0;
    }
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    if (e.key === 'Escape') {
      e.stopPropagation();
      e.preventDefault();
      this.apiProxy_.closeUI();
    }
  }

  /**
   * Handles key events when the search field has focus.
   * @param {!KeyboardEvent} e
   * @private
   */
  onSearchKeyDown_(e) {
    // In the event the search field has focus and the first item in the list is
    // selected and we receive a Shift+Tab navigation event, ensure All DOM
    // items are available so that the focus can transfer to the last item in
    // the list.
    if (e.shiftKey && e.key === 'Tab' &&
        /** @type {!InfiniteList} */ (this.$.tabsList).selected === 0) {
      /** @type {!InfiniteList} */ (this.$.tabsList)
          .ensureAllDomItemsAvailable();
      return;
    }

    // Do not interfere with the search field's management of text selection
    // that relies on the Shift key.
    if (e.shiftKey) {
      return;
    }

    if (this.getSelectedIndex() === -1) {
      // No tabs matching the search text criteria.
      return;
    }

    if (selectorNavigationKeys.includes(e.key)) {
      /** @type {!InfiniteList} */ (this.$.tabsList).navigate(e.key);

      e.stopPropagation();
      e.preventDefault();

      // TODO(tluk): Fix this to use aria-activedescendant when it's updated to
      // work with ShadowDOM elements.
      this.$.searchField.announce(
          this.ariaLabel_(this.filteredOpenTabs_[this.getSelectedIndex()]));
    } else if (e.key === 'Enter') {
      this.apiProxy_.switchToTab(
          {tabId: this.getSelectedTab_().tabId}, !!this.searchText_,
          this.getSelectedIndex());
      e.stopPropagation();
    }
  }

  /** @param {string} text */
  announceA11y_(text) {
    IronA11yAnnouncer.requestAvailability();
    this.dispatchEvent(new CustomEvent(
        'iron-announce', {bubbles: true, composed: true, detail: {text}}));
  }

  /**
   * @param {!TabData} tabData
   * @return {string}
   * @private
   */
  ariaLabel_(tabData) {
    return `${tabData.tab.title} ${tabData.hostname}`;
  }

  /**
   * @param {!Tab} tab
   * @param {boolean} inActiveWindow
   * @return {!TabData}
   * @private
   */
  tabData_(tab, inActiveWindow) {
    const hostname = new URL(tab.url).hostname;
    return /** @type {!TabData} */ ({hostname, inActiveWindow, tab});
  }

  /**
   * @param {!Array<!TabData>} tabs
   * @private
   */
  updateFilteredTabs_(tabs) {
    tabs.sort((a, b) => {
      // Move the active tab to the bottom of the list
      // because it's not likely users want to click on it.
      if (this.moveActiveTabToBottom_) {
        if (a.inActiveWindow && a.tab.active) {
          return 1;
        }
        if (b.inActiveWindow && b.tab.active) {
          return -1;
        }
      }
      return (b.tab.lastActiveTimeTicks && a.tab.lastActiveTimeTicks) ?
          Number(
              b.tab.lastActiveTimeTicks.internalValue -
              a.tab.lastActiveTimeTicks.internalValue) :
          0;
    });

    this.filteredOpenTabs_ =
        fuzzySearch(this.searchText_, tabs, this.fuzzySearchOptions_);
    this.searchResultText_ = this.getA11ySearchResultText_();
  }

  /** @return {!Tab} */
  getSelectedTab_() {
    return this.filteredOpenTabs_[this.getSelectedIndex()].tab;
  }

  /** @return {string} */
  getSearchTextForTesting() {
    return this.searchText_;
  }
}

customElements.define(TabSearchAppElement.is, TabSearchAppElement);
