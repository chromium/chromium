// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/mwb_element_shared_style.js';
import 'chrome://resources/cr_elements/mwb_shared_style.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import './infinite_list.js';
import './tab_search_group_item.js';
import './tab_search_item.js';
import './tab_search_search_field.js';
import './title_item.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {listenOnce} from 'chrome://resources/js/util.m.js';
import {Token} from 'chrome://resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {fuzzySearch} from './fuzzy_search.js';
import {InfiniteList, NO_SELECTION, selectorNavigationKeys} from './infinite_list.js';
import {ariaLabel, ItemData, TabData, TabGroupData, TabItemType, tokenEquals, tokenToString} from './tab_data.js';
import {ProfileData, RecentlyClosedTab, RecentlyClosedTabGroup, Tab, TabGroup, TabUpdateInfo, Window} from './tab_search.mojom-webui.js';
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

      /** @private {number} */
      availableHeight_: Number,

      /** @private {!Array<!TabData|!TabGroupData>} */
      filteredItems_: {
        type: Array,
        value: [],
      },

      /**
       * Options for fuzzy search. Controls how heavily weighted fields are
       * relative to each other in the scoring via field weights.
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
            },
            {
              name: 'tabGroup.title',
              weight: 1.5,
            },
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

    /** @private {!Map<string, !TabGroup>} */
    this.tabGroupsMap_ = new Map();

    /** @private {!Array<TabGroupData>} */
    this.recentlyClosedTabGroups_ = [];

    /** @private {!Array<!TabData>} */
    this.openTabs_ = [];

    /** @private {!Array<!TabData>} */
    this.recentlyClosedTabs_ = [];

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
    this.openTabsTitleItem_ = new TitleItem(loadTimeData.getString('openTabs'));

    /** @private {!TitleItem} */
    this.recentlyClosedTitleItem_ = new TitleItem(
        loadTimeData.getString('recentlyClosed'), true /*expandable*/,
        true /*expanded*/);
  }

  /** @override */
  ready() {
    super.ready();

    // Update option values for fuzzy search from feature params.
    this.fuzzySearchOptions_ = Object.assign({}, this.fuzzySearchOptions_, {
      ignoreLocation: loadTimeData.getBoolean('searchIgnoreLocation'),
      threshold: loadTimeData.getValue('searchThreshold'),
      distance: loadTimeData.getInteger('searchDistance'),
      keys: [
        {
          name: 'tab.title',
          weight: loadTimeData.getValue('searchTitleWeight'),
        },
        {
          name: 'hostname',
          weight: loadTimeData.getValue('searchHostnameWeight'),
        },
        {
          name: 'tabGroup.title',
          weight: loadTimeData.getValue('searchGroupTitleWeight'),
        },
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
            profileData => this.tabsChanged_(profileData)),
        callbackRouter.tabUpdated.addListener(
            tabUpdateInfo => this.onTabUpdated_(tabUpdateInfo)),
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
    this.filteredItems_ = [];

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
      this.tabsChanged_(profileData);
    });
  }

  /**
   * @param {!TabUpdateInfo} tabUpdateInfo
   * @private
   */
  onTabUpdated_(tabUpdateInfo) {
    const {tab, inActiveWindow} = tabUpdateInfo;
    const tabData = this.tabData_(
        tab, inActiveWindow, TabItemType.OPEN_TAB, this.tabGroupsMap_);
    // Replace the tab with the same tabId and trigger rerender.
    for (let i = 0; i < this.openTabs_.length; ++i) {
      if (this.openTabs_[i].tab.tabId === tab.tabId) {
        this.openTabs_[i] = tabData;
        this.updateFilteredTabs_();
        return;
      }
    }

    // If the updated tab's id is not found in the existing open tabs, add it
    // to the list.
    this.openTabs_.push(tabData);
    this.updateFilteredTabs_();
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

    this.filteredItems_ = this.filteredItems_.filter(
        itemData => !(itemData.tab && ids.has(itemData.tab.tabId)));
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

    this.updateFilteredTabs_();
    // Reset the selected item whenever a search query is provided.
    /** @type {!InfiniteList} */ (this.$.tabsList).selected =
        this.selectableItemCount_() > 0 ? 0 : NO_SELECTION;

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

    const itemCount = this.selectableItemCount_();
    let text;
    if (this.searchText_.length > 0) {
      text = loadTimeData.getStringF(
          itemCount == 1 ? 'a11yFoundTabFor' : 'a11yFoundTabsFor', itemCount,
          this.searchText_);
    } else {
      text = loadTimeData.getStringF(
          itemCount == 1 ? 'a11yFoundTab' : 'a11yFoundTabs', itemCount);
    }
    return text;
  }

  /**
   * @returns {number} The number of selectable list items, excludes non
   *     selectable items such as section title items.
   * @private
   */
  selectableItemCount_() {
    return this.filteredItems_.reduce((acc, item) => {
      return acc + (item instanceof TitleItem ? 0 : 1);
    }, 0);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemClick_(e) {
    const tabItem = /** @type {!ItemData} */ (e.model.item);
    this.tabItemAction_(tabItem, /** @type {number} */ (e.model.index));
  }

  /**
   * Trigger the click/press action associated with the given Tab item type.
   * @param {!ItemData} itemData
   * @param {number} tabIndex
   * @throws {Error}
   * @private
   */
  tabItemAction_(itemData, tabIndex) {
    switch (itemData.type) {
      case TabItemType.OPEN_TAB:
        this.apiProxy_.switchToTab(
            {tabId: /** @type {!TabData} */ (itemData).tab.tabId},
            !!this.searchText_, tabIndex);
        return;
      case TabItemType.RECENTLY_CLOSED_TAB:
        this.apiProxy_.openRecentlyClosedEntry(
            /** @type {!TabData} */ (itemData).tab.tabId, !!this.searchText_,
            true);
        return;
      case TabItemType.RECENTLY_CLOSED_TAB_GROUP:
        this.apiProxy_.openRecentlyClosedEntry(
            /** @type {!TabGroupData} */ (itemData).tabGroup.sessionId,
            !!this.searchText_, false);
        return;
      default:
        throw new Error('ItemData is of invalid type.');
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemClose_(e) {
    performance.mark('tab_search:close_tab:metric_begin');
    const tabId = e.model.item.tab.tabId;
    this.apiProxy_.closeTab(
        tabId, !!this.searchText_,
        /** @type {number} */ (e.model.index));
    this.announceA11y_(loadTimeData.getString('a11yTabClosed'));
    listenOnce(this.$.tabsList, 'iron-items-changed', () => {
      performance.mark('tab_search:close_tab:metric_end');
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

    const itemData = /** @type {!ItemData} */ (e.model.item);
    this.tabItemAction_(itemData, /** @type {number} */ (e.model.index));
  }

  /**
   * @param {!ProfileData} profileData
   * @private
   */
  tabsChanged_(profileData) {
    this.tabGroupsMap_ = profileData.tabGroups.reduce((map, tabGroup) => {
      map.set(tokenToString(tabGroup.id), tabGroup);
      return map;
    }, new Map());
    this.openTabs_ = profileData.windows.reduce(
        (acc, {active, tabs}) => acc.concat(tabs.map(
            tab => this.tabData_(
                tab, active, TabItemType.OPEN_TAB, this.tabGroupsMap_))),
        []);
    this.recentlyClosedTabs_ = profileData.recentlyClosedTabs.map(
        tab => this.tabData_(
            tab, false, TabItemType.RECENTLY_CLOSED_TAB, this.tabGroupsMap_));
    this.recentlyClosedTabGroups_ =
        profileData.recentlyClosedTabGroups.map(tabGroup => {
          const tabGroupData = new TabGroupData(tabGroup);
          tabGroupData.a11yTypeText =
              loadTimeData.getString('a11yRecentlyClosedTabGroup');
          return tabGroupData;
        });
    this.recentlyClosedTitleItem_.expanded =
        profileData.recentlyClosedSectionExpanded;

    this.updateFilteredTabs_();

    // If there was no previously selected index, set the first item as
    // selected; else retain the currently selected index. If the list
    // shrunk above the selected index, select the last index in the list.
    // If there are no matching results, set the selected index value to none.
    const tabsList = /** @type {!InfiniteList} */ (this.$.tabsList);
    tabsList.selected = Math.min(
        Math.max(this.getSelectedIndex(), 0), this.selectableItemCount_() - 1);
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

  /**
   * @param {CustomEvent} e
   * @private
   */
  onTitleExpandChanged_(e) {
    // Instead of relying on two-way binding to update the `expanded` property,
    // we update the value directly as the `expanded-changed` event takes place
    // before a two way bound property update and we need the TitleItem
    // instance to reflect the updated state prior to calling the
    // updateFilteredTabs_ function.
    const expanded = e.detail.value;
    const titleItem = /** @type {TitleItem} */ (e.model.item);
    titleItem.expanded = expanded;
    this.apiProxy_.saveRecentlyClosedExpandedPref(expanded);

    this.updateFilteredTabs_();
    e.stopPropagation();
  }

  /** @private */
  onSearchFocus_() {
    const tabsList = /** @type {!InfiniteList} */ (this.$.tabsList);
    if (tabsList.selected === NO_SELECTION && this.selectableItemCount_() > 0) {
      tabsList.selected = 0;
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
      // work with Shadow DOM elements.
      this.$.searchField.announce(ariaLabel(this.$.tabsList.selectedItem));
    } else if (e.key === 'Enter') {
      const itemData = /** @type {!ItemData} */ (this.$.tabsList.selectedItem);
      this.tabItemAction_(itemData, this.getSelectedIndex());
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
   * @param {!Tab|!RecentlyClosedTab} tab
   * @param {boolean} inActiveWindow
   * @param {!TabItemType} type
   * @param {!Map<string, !TabGroup>} tabGroupsMap
   * @return {!TabData}
   * @private
   */
  tabData_(tab, inActiveWindow, type, tabGroupsMap) {
    const tabData = new TabData(tab, type);
    tabData.hostname = new URL(tab.url.url).hostname;

    if (tab.groupId) {
      tabData.tabGroup = tabGroupsMap.get(tokenToString(tab.groupId));
    }
    if (type === TabItemType.OPEN_TAB) {
      tabData.inActiveWindow = inActiveWindow;
    }

    tabData.a11yTypeText = loadTimeData.getString(
        type === TabItemType.OPEN_TAB ? 'a11yOpenTab' :
                                        'a11yRecentlyClosedTab');

    return tabData;
  }

  /**
   * @param {!ItemData} itemData
   * @throws {Error}
   * @private
   */
  getRecentlyClosedItemLastActiveTime_(itemData) {
    if (itemData.type === TabItemType.RECENTLY_CLOSED_TAB &&
        itemData instanceof TabData) {
      return /** @type {!TabData} */ (itemData).tab.lastActiveTime;
    }

    if (itemData.type === TabItemType.RECENTLY_CLOSED_TAB_GROUP &&
        itemData instanceof TabGroupData) {
      return /** @type {!TabGroupData} */ (itemData).tabGroup.lastActiveTime;
    }

    throw new Error('ItemData provided is invalid.');
  }

  /** @private */
  updateFilteredTabs_() {
    this.openTabs_.sort((a, b) => {
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

    const filteredOpenTabs =
        fuzzySearch(this.searchText_, this.openTabs_, this.fuzzySearchOptions_);
    let filteredRecentlyClosedItems = fuzzySearch(
        this.searchText_,
        this.recentlyClosedTabs_.concat(this.recentlyClosedTabGroups_),
        this.fuzzySearchOptions_);
    filteredRecentlyClosedItems.sort((a, b) => {
      const aTime = this.getRecentlyClosedItemLastActiveTime_(a);
      const bTime = this.getRecentlyClosedItemLastActiveTime_(b);

      return (bTime && aTime) ?
          Number(bTime.internalValue - aTime.internalValue) :
          0;
    });

    // Limit the number of recently closed items to the default display count
    // when no search text has been specified. Filter out recently closed tabs
    // that belong to a recently closed tab group by default.
    const recentlyClosedTabGroupIds = this.recentlyClosedTabGroups_.reduce(
        (acc, tabGroupData) => acc.concat(tabGroupData.tabGroup.id), []);
    if (!this.searchText_.length) {
      filteredRecentlyClosedItems =
          filteredRecentlyClosedItems
              .filter((recentlyClosedItem) => {
                return (
                    recentlyClosedItem instanceof TabGroupData ||
                    !/** @type {!RecentlyClosedTab} */ (recentlyClosedItem.tab)
                         .groupId ||
                    !recentlyClosedTabGroupIds.some(
                        groupId => tokenEquals(
                            groupId, /** @type {!Token} */
                            (        /** @type {!RecentlyClosedTab} */
                             (recentlyClosedItem.tab).groupId))));
              })
              .slice(0, this.recentlyClosedDefaultItemDisplayCount_);
    }

    this.filteredItems_ = [
      [this.openTabsTitleItem_, filteredOpenTabs],
      [this.recentlyClosedTitleItem_, filteredRecentlyClosedItems],
    ].reduce((acc, [sectionTitle, sectionItems]) => {
      if (sectionItems.length !== 0) {
        acc.push(sectionTitle);
        if (!sectionTitle.expandable ||
            sectionTitle.expandable && sectionTitle.expanded) {
          acc.push(...sectionItems);
        }
      }
      return acc;
    }, []);
    this.searchResultText_ = this.getA11ySearchResultText_();
  }

  /** @return {string} */
  getSearchTextForTesting() {
    return this.searchText_;
  }
}

customElements.define(TabSearchAppElement.is, TabSearchAppElement);
