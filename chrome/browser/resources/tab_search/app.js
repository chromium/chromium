// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
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
import {Tab, WindowTabs} from './tab_search.mojom-webui.js';
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

      /** @private {?Array<!WindowTabs>} */
      openTabs_: {
        type: Array,
        observer: 'openTabsChanged_',
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

    // TODO(tluk): The listener should provide the data needed to update the
    // WebUI without having to make another round trip request to the Browser.
    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.tabsChanged.addListener(() => this.updateTabs_()),
        callbackRouter.tabUpdated.addListener(tab => this.onTabUpdated_(tab)),
        callbackRouter.tabsRemoved.addListener(
            tabIds => this.onTabsRemoved_(tabIds)));
    this.updateTabs_();
  }

  /** @override */
  disconnectedCallback() {
    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));
  }

  /** @private */
  updateTabs_() {
    const getTabsStartTimestamp = Date.now();
    this.apiProxy_.getProfileTabs().then(({profileTabs}) => {
      chrome.metricsPrivate.recordTime(
          'Tabs.TabSearch.WebUI.TabListDataReceived',
          Math.round(Date.now() - getTabsStartTimestamp));

      // Prior to the first load |this.openTabs_| has not been set. Record the
      // time it takes for the initial list of tabs to render.
      if (!this.openTabs_) {
        listenOnce(this.$.tabsList, 'dom-change', () => {
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
   * @param {!Tab} updatedTab
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

  /**
   * @param {!Array<number>} tabIds
   * @private
   */
  onTabsRemoved_(tabIds) {
    const windows = this.openTabs_;
    if (windows) {
      const ids = new Set(tabIds);
      for (const window of windows) {
        window.tabs = window.tabs.filter(tab => (!ids.has(tab.tabId)));
      }
      this.openTabs_ = windows.concat();
    }
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

    this.updateFilteredTabs_(this.openTabs_ || []);
    // Reset the selected item whenever a search query is provided.
    /** @type {!InfiniteList} */ (this.$.tabsList).selected =
        this.filteredOpenTabs_.length > 0 ? 0 : NO_SELECTION;

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
    this.announceA11y_(text);
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
    const tabIndex = e.currentTarget.parentNode.indexOf(e.currentTarget);
    const tabId = Number.parseInt(e.currentTarget.id, 10);
    this.apiProxy_.switchToTab({tabId}, !!this.searchText_, tabIndex);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemClose_(e) {
    performance.mark('close_tab:benchmark_begin');
    const tabIndex = e.currentTarget.parentNode.indexOf(e.currentTarget);
    const tabId = Number.parseInt(e.currentTarget.id, 10);
    this.apiProxy_.closeTab(tabId, !!this.searchText_, tabIndex);
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
   * @param {!Array<!WindowTabs>} newOpenTabs
   * @private
   */
  openTabsChanged_(newOpenTabs) {
    this.updateFilteredTabs_(newOpenTabs);

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
        e.currentTarget.parentNode.indexOf(e.currentTarget);
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

      // For some reasons setting combobox/aria-activedescendant on
      // tab-search-search-field has no effect, so manually announce a11y
      // message here.
      this.announceA11y_(
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
   * @param {!Array<!WindowTabs>} windowTabs
   * @private
   */
  updateFilteredTabs_(windowTabs) {
    const result = [];
    windowTabs.forEach(window => {
      window.tabs.forEach(tab => {
        const hostname = new URL(tab.url).hostname;
        const inActiveWindow = window.active;
        result.push({hostname, inActiveWindow, tab});
      });
    });
    result.sort((a, b) => {
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
        fuzzySearch(this.searchText_, result, this.fuzzySearchOptions_);
  }

  /** return {!Tab} */
  getSelectedTab_() {
    return this.filteredOpenTabs_[this.getSelectedIndex()].tab;
  }
}

customElements.define(TabSearchAppElement.is, TabSearchAppElement);
