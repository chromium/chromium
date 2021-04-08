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
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {listenOnce} from 'chrome://resources/js/util.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {fuzzySearch} from './fuzzy_search.js';
import {InfiniteList, NO_SELECTION, selectorNavigationKeys} from './infinite_list.js';
import {ariaLabel, TabData, TabItemType} from './tab_data.js';
import {Tab, Window} from './tab_search.mojom-webui.js';
import {TabSearchApiProxy, TabSearchApiProxyImpl} from './tab_search_api_proxy.js';
import {TitleItem} from './title_item.js';

// The minimum number of list items we allow viewing regardless of browser
// height. Includes a half row that hints to the user the capability to scroll.
/** @type {number} */
const MINIMUM_AVAILABLE_HEIGHT_LIST_ITEM_COUNT = 5.5;

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
      recentlyClosedTabs_: {
        type: Array,
        value: [],
      },

      /** @private {number} */
      availableHeight_: Number,

      /** @private {!Array<!TabData>} */
      filteredOpenTabs_: {
        type: Array,
        value: [],
      },

      /** @private {!Array<!TabData>} */
      filteredRecentlyClosedTabs_: {
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
      moveActiveTabToBottom_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('moveActiveTabToBottom'),
      },

      recentlyClosedDefaultItemDisplayCount_: {
        type: Number,
        value: () =>
            /** @type {number} */ (
                loadTimeData.getValue('recentlyClosedDefaultItemDisplayCount')),
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

    /** @private {!TitleItem} */
    this.openTabsTitleItem_ =
        new TitleItem(loadTimeData.getStringF('openTabs'));

    /** @private {!TitleItem} */
    this.recentlyClosedTabsTitleItem_ = new TitleItem(
        loadTimeData.getStringF('recentlyClosedTabs'));
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
            profileData => this.tabsChanged_(
                profileData.windows, profileData.recentlyClosedTabs)),
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

  /**
   * @param {string} name A property whose value is specified in pixels.
   * @return {number}
   */
  getStylePropertyPixelValue_(name) {
    const pxValue = getComputedStyle(this).getPropertyValue(name);
    assert(pxValue);

    return Number.parseInt(pxValue.trim().slice(0, -2), 10);
  }

  /**
   * Calculate the list's available height by subtracting the height used by
   * the search and feedback fields.
   *
   * @param {number} height
   * @return {number}
   * @private
   */
  listMaxHeight_(height) {
    return Math.max(
        height - this.$.searchField.offsetHeight,
        Math.round(
            MINIMUM_AVAILABLE_HEIGHT_LIST_ITEM_COUNT *
            this.getStylePropertyPixelValue_('--mwb-item-height')));
  }

  /** @private */
  onDocumentHidden_() {
    this.$.tabsList.scrollTop = 0;
    this.$.tabsList.selected = NO_SELECTION;

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

      // The infinite-list produces viewport-filled events whenever a data or
      // scroll position change triggers the the viewport fill logic.
      listenOnce(this.$.tabsList, 'viewport-filled', () => {
        // Push showUI() to the event loop to allow reflow to occur following
        // the DOM update.
        setTimeout(() => this.apiProxy_.showUI(), 0);
      });

      this.availableHeight_ = profileData.windows.find((t) => t.active).height;
      this.tabsChanged_(profileData.windows, profileData.recentlyClosedTabs);
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
        this.openTabs_[i] = this.tabData_(
            updatedTab, this.openTabs_[i].inActiveWindow, TabItemType.OPEN);
        this.updateFilteredTabs_(this.openTabs_, this.recentlyClosedTabs_);
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

    this.updateFilteredTabs_(this.openTabs_, this.recentlyClosedTabs_);
    // Reset the selected item whenever a search query is provided.
    /** @type {!InfiniteList} */ (this.$.tabsList).selected =
        (this.filteredOpenTabs_.length +
         this.filteredRecentlyClosedTabs_.length) > 0 ?
        0 :
        NO_SELECTION;

    this.$.searchField.announce(this.getA11ySearchResultText_());
  }

  /**
   * @return {string}
   * @private
   */
  getA11ySearchResultText_() {
    // TODO(romanarora): Screen readers' list item number announcement will
    // not match as it counts the title items too. Investigate how to
    // programmatically control announcements to avoid this.
    const length =
        this.filteredOpenTabs_.length + this.filteredRecentlyClosedTabs_.length;
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

  /**
   * @param {!Event} e
   * @private
   */
  onItemClick_(e) {
    const tabData = /** @type {!TabData} */ (e.model.item);
    this.tabItemAction_(
        tabData.type, tabData.tab.tabId, /** @type {number} */ (e.model.index));
  }

  /**
   * Trigger the click/press action associated with the given Tab item type for
   * the given Tab Id.
   * @param {!TabItemType} type
   * @param {number} tabId
   * @param {number} tabIndex
   * @private
   */
  tabItemAction_(type, tabId, tabIndex) {
    if (type === TabItemType.OPEN) {
      this.apiProxy_.switchToTab({tabId}, !!this.searchText_, tabIndex);
    } else {
      this.apiProxy_.openRecentlyClosedTab(tabId);
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemClose_(e) {
    performance.mark('close_tab:benchmark_begin');
    const tabId = e.model.item.tab.tabId;
    this.apiProxy_.closeTab(
        tabId, !!this.searchText_,
        /** @type {number} */ (e.model.index));
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

    const tabData = /** @type {!TabData} */ (e.model.item);
    this.tabItemAction_(
        tabData.type, tabData.tab.tabId, /** @type {number} */ (e.model.index));
  }

  /**
   * @param {!Array<!Window>} newOpenWindows
   * @param {!Array<!Tab>} recentlyClosedTabs
   * @private
   */
  tabsChanged_(newOpenWindows, recentlyClosedTabs) {
    this.openTabs_ = newOpenWindows.reduce(
        (acc, {active, tabs}) => acc.concat(
            tabs.map(tab => this.tabData_(tab, active, TabItemType.OPEN))),
        []);
    this.recentlyClosedTabs_ = recentlyClosedTabs.map(
        tab => this.tabData_(tab, false, TabItemType.RECENTLY_CLOSED));
    this.updateFilteredTabs_(this.openTabs_, this.recentlyClosedTabs_);

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
      this.$.searchField.announce(ariaLabel(this.$.tabsList.selectedItem));
    } else if (e.key === 'Enter') {
      const tabData = /** @type {!TabData} */ (this.$.tabsList.selectedItem);
      this.tabItemAction_(
          tabData.type, tabData.tab.tabId, this.getSelectedIndex());
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
    return ariaLabel(tabData);
  }

  /**
   * @param {!Array<!TabData>} filteredOpenTabs
   * @param {!Array<!TabData>} filteredRecentlyClosedTabs
   * @return {number}
   * @private
   */
  filteredTabItemsCount_(filteredOpenTabs, filteredRecentlyClosedTabs) {
    return filteredOpenTabs.length + filteredRecentlyClosedTabs.length;
  }

  /**
   * @param {!Array<!TabData>} filteredOpenTabs
   * @param {!Array<!TabData>} filteredRecentlyClosedTabs
   * @return {!Array<!TabData|!TitleItem>}
   * @private
   */
  listItems_(filteredOpenTabs, filteredRecentlyClosedTabs) {
    const items = [];
    if (filteredOpenTabs.length !== 0) {
      items.push(this.openTabsTitleItem_, ...filteredOpenTabs);
    }

    if (filteredRecentlyClosedTabs.length !== 0) {
      items.push(
          this.recentlyClosedTabsTitleItem_, ...filteredRecentlyClosedTabs);
    }

    return items;
  }

  /**
   * @param {!Tab} tab
   * @param {boolean} inActiveWindow
   * @param {!TabItemType} type
   * @return {!TabData}
   * @private
   */
  tabData_(tab, inActiveWindow, type) {
    const tabData = new TabData();
    try {
      tabData.hostname = new URL(tab.url).hostname;
    } catch (e) {
      // TODO(crbug.com/1186409): Remove this after we root cause the issue
      console.error(`Error parsing URL on Tab Search: url=${tab.url}`);
      tabData.hostname = '';
    }
    tabData.inActiveWindow = inActiveWindow;
    tabData.tab = tab;
    tabData.type = type;
    tabData.a11yTypeText = loadTimeData.getStringF(
        type === TabItemType.OPEN ? 'openTabs' : 'recentlyClosedTabs');
    return tabData;
  }

  /**
   * @param {!Array<!TabData>} openTabs
   * @param {!Array<!TabData>} recentlyClosedTabs
   * @private
   */
  updateFilteredTabs_(openTabs, recentlyClosedTabs) {
    openTabs.sort((a, b) => {
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
        fuzzySearch(this.searchText_, openTabs, this.fuzzySearchOptions_);
    const filteredRecentlyClosedTabs = fuzzySearch(
        this.searchText_, recentlyClosedTabs, this.fuzzySearchOptions_);
    this.filteredRecentlyClosedTabs_ = this.searchText_.length ?
        filteredRecentlyClosedTabs :
        filteredRecentlyClosedTabs.slice(
            0, this.recentlyClosedDefaultItemDisplayCount_);
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
