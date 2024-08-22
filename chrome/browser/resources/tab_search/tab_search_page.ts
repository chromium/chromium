// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './selectable_lazy_list.js';
import './strings.m.js';
import './tab_search_group_item.js';
import './tab_search_item.js';
import './title_item.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrSearchFieldMixinLit} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {MetricsReporter} from 'chrome://resources/js/metrics_reporter/metrics_reporter.js';
import {MetricsReporterImpl} from 'chrome://resources/js/metrics_reporter/metrics_reporter.js';
import {listenOnce} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Token} from 'chrome://resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';

import type {SearchOptions} from './search.js';
import {search} from './search.js';
import type {SelectableLazyListElement} from './selectable_lazy_list.js';
import {NO_SELECTION, selectorNavigationKeys} from './selectable_lazy_list.js';
import {ariaLabel, getHostname, getTabGroupTitle, getTitle, type ItemData, normalizeURL, TabData, TabGroupData, TabItemType, tokenEquals, tokenToString} from './tab_data.js';
import type {ProfileData, RecentlyClosedTab, RecentlyClosedTabGroup, Tab, TabGroup, TabsRemovedInfo, TabUpdateInfo} from './tab_search.mojom-webui.js';
import type {TabSearchApiProxy} from './tab_search_api_proxy.js';
import {TabSearchApiProxyImpl} from './tab_search_api_proxy.js';
import type {TabSearchGroupItemElement} from './tab_search_group_item.js';
import type {TabSearchItemElement} from './tab_search_item.js';
import {getCss} from './tab_search_page.css.js';
import {getHtml} from './tab_search_page.html.js';
import {tabHasMediaAlerts} from './tab_search_utils.js';
import {TitleItem} from './title_item.js';

// The minimum number of list items we allow viewing regardless of browser
// height. Includes a half row that hints to the user the capability to scroll.
const MINIMUM_AVAILABLE_HEIGHT_LIST_ITEM_COUNT: number = 5.5;

const TabSearchSearchFieldBase = CrSearchFieldMixinLit(CrLitElement);

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 */
export enum TabSwitchAction {
  WITHOUT_SEARCH = 0,
  WITH_SEARCH = 1,
}

export interface TabSearchPageElement {
  $: {
    divider: HTMLElement,
    searchField: HTMLElement,
    searchInput: HTMLInputElement,
    searchWrapper: HTMLElement,
    tabsList: SelectableLazyListElement,
  };
}

export class TabSearchPageElement extends TabSearchSearchFieldBase {
  static get is() {
    return 'tab-search-page';
  }

  static override get properties() {
    return {
      // Text that describes the resulting tabs currently present in the list.
      searchResultText_: {type: String},
      shortcut_: {type: String},
      searchText_: {type: String},
      availableHeight_: {type: Number},
      filteredItems_: {type: Array},
      listMaxHeight_: {type: Number},
      listItemSize_: {type: Number},

      /**
       * Options for search. Controls how heavily weighted fields are relative
       * to each other in the scoring via field weights.
       */
      searchOptions_: {type: Object},
      recentlyClosedDefaultItemDisplayCount_: {type: Number},

      tabOrganizationEnabled: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  tabOrganizationEnabled: boolean =
      loadTimeData.getBoolean('tabOrganizationEnabled');
  private searchText_: string = '';
  private availableHeight_?: number;
  protected listMaxHeight_?: number;
  protected listItemSize_?: number;
  protected filteredItems_: Array<TitleItem|TabData|TabGroupData> = [];
  private searchOptions_: SearchOptions = {
    includeScore: true,
    includeMatches: true,
    ignoreLocation: false,
    threshold: 0.0,
    distance: 200,
    keys:
        [
          {
            name: 'tab.title',
            getter: getTitle,
            weight: 2,
          },
          {
            name: 'hostname',
            getter: getHostname,
            weight: 1,
          },
          {
            name: 'tabGroup.title',
            getter: getTabGroupTitle,
            weight: 1.5,
          },
        ],
  };
  private recentlyClosedDefaultItemDisplayCount_: number =
      loadTimeData.getValue('recentlyClosedDefaultItemDisplayCount');
  protected searchResultText_: string = '';
  protected activeSelectionId_?: string;
  protected shortcut_: string = loadTimeData.getString('shortcutText');
  override autofocus: boolean = false;

  private apiProxy_: TabSearchApiProxy = TabSearchApiProxyImpl.getInstance();
  private metricsReporter_: MetricsReporter|null = null;
  private listenerIds_: number[] = [];
  private tabGroupsMap_: Map<string, TabGroup> = new Map();
  private recentlyClosedTabGroups_: TabGroupData[] = [];
  private openTabs_: TabData[] = [];
  private recentlyClosedTabs_: TabData[] = [];
  private windowShownTimestamp_: number = Date.now();
  private mediaTabsTitleItem_: TitleItem;
  private openTabsTitleItem_: TitleItem;
  private recentlyClosedTitleItem_: TitleItem;
  private filteredOpenTabsCount_: number = 0;
  private filteredOpenHeaderIndices_: number[] = [];
  private initiallySelectedIndex_: number = NO_SELECTION;
  private documentVisibilityChangedListener_: () => void;
  private elementVisibilityChangedListener_: IntersectionObserver;
  private wasInactive_: boolean = loadTimeData.getInteger('tabIndex') !== 0;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();

    this.documentVisibilityChangedListener_ = () => {
      if (document.visibilityState === 'visible') {
        this.windowShownTimestamp_ = Date.now();
        this.updateTabs_();
      } else {
        this.onDocumentHidden_();
      }
    };

    this.elementVisibilityChangedListener_ =
        new IntersectionObserver((entries, _observer) => {
          entries.forEach(entry => {
            this.onElementVisibilityChanged_(entry.intersectionRatio > 0);
          });
        }, {root: document.documentElement});

    this.mediaTabsTitleItem_ =
        new TitleItem(loadTimeData.getString('mediaTabs'));

    this.openTabsTitleItem_ = new TitleItem(loadTimeData.getString('openTabs'));

    this.recentlyClosedTitleItem_ = new TitleItem(
        loadTimeData.getString('recentlyClosed'), true /*expandable*/,
        true /*expanded*/);
  }

  get metricsReporter(): MetricsReporter {
    if (!this.metricsReporter_) {
      this.metricsReporter_ = MetricsReporterImpl.getInstance();
    }
    return this.metricsReporter_;
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.listItemSize_ = this.getStylePropertyPixelValue_('--mwb-item-height');
  }

  override connectedCallback() {
    super.connectedCallback();

    document.addEventListener(
        'visibilitychange', this.documentVisibilityChangedListener_);

    this.elementVisibilityChangedListener_.observe(this);

    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.tabsChanged.addListener(this.tabsChanged_.bind(this)),
        callbackRouter.tabUpdated.addListener(this.onTabUpdated_.bind(this)),
        callbackRouter.tabsRemoved.addListener(this.onTabsRemoved_.bind(this)));

    // If added in a visible state update current tabs.
    if (document.visibilityState === 'visible') {
      this.updateTabs_();
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));

    document.removeEventListener(
        'visibilitychange', this.documentVisibilityChangedListener_);

    this.elementVisibilityChangedListener_.disconnect();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('availableHeight_')) {
      assert(this.availableHeight_ !== undefined);

      /**
       * Calculate the list's available height by subtracting the height used by
       * the search and feedback fields.
       */
      this.listMaxHeight_ = Math.max(
          this.availableHeight_ - this.$.searchField.offsetHeight -
              this.$.divider.offsetHeight,
          Math.round(
              MINIMUM_AVAILABLE_HEIGHT_LIST_ITEM_COUNT *
              this.getStylePropertyPixelValue_('--mwb-item-height')));
    }
  }

  override getSearchInput(): HTMLInputElement {
    return this.$.searchInput;
  }

  /**
   * Do not schedule the timer from CrSearchFieldMixin to make search more
   * responsive.
   */
  override onSearchTermInput() {
    this.hasSearchText = this.getSearchInput().value !== '';
    this.searchText_ = this.getSearchInput().value;
    // Reset the selected item whenever a search query is provided.
    // updateFilteredTabs_ will set the correct tab index for initial selection.
    const tabsList = this.$.tabsList;
    tabsList.resetSelected();

    this.updateFilteredTabs_();

    // http://crbug.com/1481787: Dispatch the search event to update the
    // internal value to make CrSearchFieldMixin function correctly.
    this.getSearchInput().dispatchEvent(
        new CustomEvent('search', {composed: true, detail: this.searchText_}));
  }

  /**
   * @param name A property whose value is specified in pixels.
   */
  private getStylePropertyPixelValue_(name: string): number {
    const pxValue = getComputedStyle(this).getPropertyValue(name);
    assert(pxValue);

    return Number.parseInt(pxValue.trim().slice(0, -2), 10);
  }

  private onDocumentHidden_() {
    this.filteredItems_ = [];

    this.setValue('');
    this.$.searchInput.focus();
  }

  private onElementVisibilityChanged_(visible: boolean) {
    if (visible && this.wasInactive_) {
      this.$.tabsList.fillCurrentViewport();
    } else if (!visible) {
      this.wasInactive_ = true;
    }
  }

  private updateTabs_() {
    const isMarkOverlap =
        this.metricsReporter.hasLocalMark('TabListDataReceived');
    chrome.metricsPrivate.recordBoolean(
        'Tabs.TabSearch.WebUI.TabListDataReceived2.IsOverlap', isMarkOverlap);
    if (!isMarkOverlap) {
      this.metricsReporter.mark('TabListDataReceived');
    }

    this.apiProxy_.getProfileData().then(({profileData}) => {
      // TODO(crbug.com/40205026): this is a side-by-side comparison of metrics
      // reporter histogram vs. old histogram. Cleanup when the experiment ends.
      this.metricsReporter.measure('TabListDataReceived')
          .then(
              e => this.metricsReporter.umaReportTime(
                  'Tabs.TabSearch.WebUI.TabListDataReceived2', e))
          .then(() => this.metricsReporter.clearMark('TabListDataReceived'))
          // Ignore silently if mark 'TabListDataReceived' is missing.
          .catch(() => {});

      // In rare cases there is no browser window. I suspect this happens during
      // browser shutdown. Don't show Tab Search when this happens.
      if (!profileData.windows) {
        console.warn('Tab Search: no browser window.');
        return;
      }

      // TODO(crbug.com/40855872): Determine why no active window is reported
      // in some cases on ChromeOS and Linux.
      const activeWindow = profileData.windows.find((t) => t.active);
      this.availableHeight_ =
          activeWindow ? activeWindow!.height : profileData.windows[0]!.height;

      // The selectable-list produces viewport-filled events whenever a data
      // or scroll position change triggers the viewport fill logic.
      listenOnce(
          this.$.tabsList, 'viewport-filled',
          () => this.apiProxy_.notifySearchUiReadyToShow());

      this.tabsChanged_(profileData);
    });
  }

  private onTabUpdated_(tabUpdateInfo: TabUpdateInfo) {
    const {tab, inActiveWindow} = tabUpdateInfo;
    const tabData = this.tabData_(
        tab, inActiveWindow, TabItemType.OPEN_TAB, this.tabGroupsMap_);
    // Replace the tab with the same tabId and trigger rerender.
    let foundTab = false;
    for (let i = 0; i < this.openTabs_.length && !foundTab; ++i) {
      if (this.openTabs_[i]!.tab.tabId === tab.tabId) {
        this.openTabs_[i] = tabData;
        this.updateFilteredTabs_();
        foundTab = true;
      }
    }

    // If the updated tab's id is not found in the existing open tabs, add it
    // to the list.
    if (!foundTab) {
      this.openTabs_.push(tabData);
      this.updateFilteredTabs_();
    }

    this.metricsReporter.measure('TabUpdated')
        .then(
            e => this.metricsReporter.umaReportTime(
                'Tabs.TabSearch.Mojo.TabUpdated', e))
        .then(() => this.metricsReporter.clearMark('TabUpdated'))
        // Ignore silently if mark 'TabUpdated' is missing.
        .catch(() => {});
  }

  private onTabsRemoved_(tabsRemovedInfo: TabsRemovedInfo) {
    if (this.openTabs_.length === 0) {
      return;
    }

    const ids = new Set(tabsRemovedInfo.tabIds);
    // Splicing in descending index order to avoid affecting preceding indices
    // that are to be removed.
    for (let i = this.openTabs_.length - 1; i >= 0; i--) {
      if (ids.has(this.openTabs_[i]!.tab.tabId)) {
        this.openTabs_.splice(i, 1);
      }
    }

    tabsRemovedInfo.recentlyClosedTabs.forEach(tab => {
      this.recentlyClosedTabs_.unshift(this.tabData_(
          tab, false, TabItemType.RECENTLY_CLOSED_TAB, this.tabGroupsMap_));
    });

    this.updateFilteredTabs_();
  }

  private itemIndexToTabIndex_(itemIndex: number) {
    // Note: the array being searched has length at most 3.
    const numPreviousHeaders =
        this.filteredOpenHeaderIndices_.findLastIndex(idx => idx < itemIndex) +
        1;
    return itemIndex - numPreviousHeaders;
  }

  getSelectedTabIndex(): number {
    return this.itemIndexToTabIndex_(this.$.tabsList.selected);
  }

  private getA11ySearchResultText_(): string {
    // TODO(romanarora): Screen readers' list item number announcement will
    // not match as it counts the title items too. Investigate how to
    // programmatically control announcements to avoid this.

    const itemCount = this.selectableItemCount_();
    let text;
    if (this.searchText_.length > 0) {
      text = loadTimeData.getStringF(
          itemCount === 1 ? 'a11yFoundTabFor' : 'a11yFoundTabsFor', itemCount,
          this.searchText_);
    } else {
      text = loadTimeData.getStringF(
          itemCount === 1 ? 'a11yFoundTab' : 'a11yFoundTabs', itemCount);
    }
    return text;
  }

  /**
   * @return The number of selectable list items, excludes non
   *     selectable items such as section title items.
   */
  private selectableItemCount_(): number {
    return this.filteredItems_.reduce((acc, item) => {
      return acc + (item instanceof TitleItem ? 0 : 1);
    }, 0);
  }

  /**
   * @return The last selectable list item, excludes non
   *     selectable items such as section title items.
   */
  private lastSelectableIndex_(): number {
    return this.filteredItems_.findLastIndex(
               item => !(item instanceof TitleItem)) ||
        -1;
  }

  protected onItemClick_(e: Event) {
    const target =
        e.currentTarget as TabSearchItemElement | TabSearchGroupItemElement;
    const tabItem = target.data;
    const tabIndex = this.itemIndexToTabIndex_(Number(target.dataset['index']));
    this.tabItemAction_(tabItem, tabIndex);
  }

  private recordMetricsForAction(action: string, tabIndex: number) {
    const withSearch = !!this.searchText_;
    if (action === 'SwitchTab') {
      chrome.metricsPrivate.recordEnumerationValue(
          'Tabs.TabSearch.WebUI.TabSwitchAction',
          withSearch ? TabSwitchAction.WITH_SEARCH :
                       TabSwitchAction.WITHOUT_SEARCH,
          Object.keys(TabSwitchAction).length);
    }
    chrome.metricsPrivate.recordSmallCount(
        withSearch ? `Tabs.TabSearch.WebUI.IndexOf${action}InFilteredList` :
                     `Tabs.TabSearch.WebUI.IndexOf${action}InUnfilteredList`,
        tabIndex);
  }

  /**
   * Trigger the click/press action associated with the given Tab item type.
   */
  private tabItemAction_(itemData: ItemData, tabIndex: number) {
    const state = this.searchText_ ? 'Filtered' : 'Unfiltered';
    let action;
    switch (itemData.type) {
      case TabItemType.OPEN_TAB:
        const isMarkOverlap = this.metricsReporter.hasLocalMark('SwitchToTab');
        chrome.metricsPrivate.recordBoolean(
            'Tabs.TabSearch.Mojo.SwitchToTab.IsOverlap', isMarkOverlap);
        if (!isMarkOverlap) {
          this.metricsReporter.mark('SwitchToTab');
        }

        this.recordMetricsForAction('SwitchTab', tabIndex);
        this.apiProxy_.switchToTab({tabId: (itemData as TabData).tab.tabId});
        action = 'SwitchTab';
        break;
      case TabItemType.RECENTLY_CLOSED_TAB:
        this.apiProxy_.openRecentlyClosedEntry(
            (itemData as TabData).tab.tabId, !!this.searchText_, true,
            tabIndex - this.filteredOpenTabsCount_);
        action = 'OpenRecentlyClosedEntry';
        break;
      case TabItemType.RECENTLY_CLOSED_TAB_GROUP:
        this.apiProxy_.openRecentlyClosedEntry(
            ((itemData as TabGroupData).tabGroup as RecentlyClosedTabGroup)
                .sessionId,
            !!this.searchText_, false, tabIndex - this.filteredOpenTabsCount_);
        action = 'OpenRecentlyClosedEntry';
        break;
      default:
        throw new Error('ItemData is of invalid type.');
    }
    chrome.metricsPrivate.recordTime(
        `Tabs.TabSearch.WebUI.TimeTo${action}In${state}List`,
        Math.round(Date.now() - this.windowShownTimestamp_));
  }

  protected onItemClose_(e: Event) {
    performance.mark('tab_search:close_tab:metric_begin');
    const target = e.currentTarget as TabSearchItemElement;
    const tabItem = target.data;
    const tabIndex = this.itemIndexToTabIndex_(Number(target.dataset['index']));
    const tabId = tabItem.tab.tabId;
    this.recordMetricsForAction('CloseTab', tabIndex);
    this.apiProxy_.closeTab(tabId);
    this.announceA11y_(loadTimeData.getString('a11yTabClosed'));
    listenOnce(this.$.tabsList, 'rendered-items-changed', () => {
      performance.mark('tab_search:close_tab:metric_end');
    });
  }

  protected onItemKeyDown_(e: KeyboardEvent) {
    if (e.key !== 'Enter' && e.key !== ' ') {
      return;
    }

    e.stopPropagation();
    e.preventDefault();

    const target =
        e.currentTarget as TabSearchItemElement | TabSearchGroupItemElement;
    const itemData = target.data;
    const tabIndex = this.itemIndexToTabIndex_(Number(target.dataset['index']));
    this.tabItemAction_(itemData, tabIndex);
  }

  private tabsChanged_(profileData: ProfileData) {
    this.tabGroupsMap_ = profileData.tabGroups.reduce((map, tabGroup) => {
      map.set(tokenToString(tabGroup.id), tabGroup);
      return map;
    }, new Map());
    this.openTabs_ = profileData.windows.reduce(
        (acc, {active, tabs}) => acc.concat(tabs.map(
            tab => this.tabData_(
                tab, active, TabItemType.OPEN_TAB, this.tabGroupsMap_))),
        [] as TabData[]);
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

    this.$.tabsList.expandedList = profileData.recentlyClosedSectionExpanded;

    this.updateFilteredTabs_();
  }

  protected onItemFocus_(e: Event) {
    // Ensure that when a TabSearchItem receives focus, it becomes the selected
    // item in the list.
    const target =
        e.currentTarget as TabSearchItemElement | TabSearchGroupItemElement;
    const index = Number(target.dataset['index']);
    this.$.tabsList.setSelected(index);
  }

  private getTitleItemFromTitle_(title: string): TitleItem {
    const item = [
      this.mediaTabsTitleItem_,
      this.openTabsTitleItem_,
      this.recentlyClosedTitleItem_,
    ].find(item => item.title === title);
    assert(item);
    return item;
  }

  protected async onTitleExpandChanged_(e: CustomEvent<{value: boolean}>) {
    // Instead of relying on two-way binding to update the `expanded` property,
    // we update the value directly as the `expanded-changed` event takes place
    // before a two way bound property update and we need the TitleItem
    // instance to reflect the updated state prior to calling the
    // updateFilteredTabs_ function.
    const expanded = e.detail.value;
    const target = e.currentTarget as HTMLElement;
    const title = target.dataset['title'];
    const index = Number(target.dataset['index']);
    assert(title);
    const titleItem = this.getTitleItemFromTitle_(title);
    if (titleItem.expanded === expanded) {
      return;
    }
    titleItem.expanded = expanded;
    this.apiProxy_.saveRecentlyClosedExpandedPref(expanded);

    this.$.tabsList.toggleAttribute('expanded-list', expanded);

    e.stopPropagation();
    await this.updateFilteredTabs_();

    // If a section's title item is the last visible element in the list and the
    // list's height is at its maximum, it will not be evident to the user that
    // on expanding the section there are now section tab items available. By
    // ensuring the first element of the section is visible, we can avoid this
    // confusion.
    if (expanded) {
      this.$.tabsList.scrollIndexIntoView(index + 1);
    }
  }

  /**
   * Handles key events when the search field has focus.
   */
  protected onSearchKeyDown_(e: KeyboardEvent) {
    // In the event the search field has focus and the first item in the list is
    // selected and we receive a Shift+Tab navigation event, ensure All DOM
    // items are available so that the focus can transfer to the last item in
    // the list.
    if (e.shiftKey && e.key === 'Tab' && this.getSelectedTabIndex() === 0) {
      this.$.tabsList.ensureAllDomItemsAvailable();
      return;
    }

    // Do not interfere with the search field's management of text selection
    // that relies on the Shift key.
    if (e.shiftKey) {
      return;
    }

    if (this.$.tabsList.selected === -1) {
      // No tabs matching the search text criteria.
      return;
    }

    if (selectorNavigationKeys.includes(e.key)) {
      this.$.tabsList.navigate(e.key);

      e.stopPropagation();
      e.preventDefault();

    } else if (e.key === 'Enter') {
      if (this.$.tabsList.selectedItem) {
        const itemData = (this.$.tabsList.selectedItem as TabSearchItemElement |
                          TabSearchGroupItemElement)
                             .data;
        this.tabItemAction_(itemData, this.getSelectedTabIndex());
      }
      e.stopPropagation();
    }
  }

  private announceA11y_(text: string) {
    getAnnouncerInstance().announce(text);
  }

  protected ariaLabel_(tabData: TabData): string {
    return ariaLabel(tabData);
  }

  private tabData_(
      tab: Tab|RecentlyClosedTab, inActiveWindow: boolean, type: TabItemType,
      tabGroupsMap: Map<string, TabGroup>): TabData {
    const tabData =
        new TabData(tab, type, new URL(normalizeURL(tab.url.url)).hostname);

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

  private getRecentlyClosedItemLastActiveTime_(itemData: ItemData) {
    if (itemData.type === TabItemType.RECENTLY_CLOSED_TAB &&
        itemData instanceof TabData) {
      return (itemData.tab as RecentlyClosedTab).lastActiveTime;
    }

    if (itemData.type === TabItemType.RECENTLY_CLOSED_TAB_GROUP &&
        itemData instanceof TabGroupData) {
      return (itemData.tabGroup as RecentlyClosedTabGroup).lastActiveTime;
    }

    throw new Error('ItemData provided is invalid.');
  }

  private async updateFilteredTabs_() {
    this.openTabs_.sort((a, b) => {
      const tabA = a.tab as Tab;
      const tabB = b.tab as Tab;
      // Move the active tab to the bottom of the list
      // because it's not likely users want to click on it.
      if (a.inActiveWindow && tabA.active) {
        return 1;
      }
      if (b.inActiveWindow && tabB.active) {
        return -1;
      }
      return (tabB.lastActiveTimeTicks && tabA.lastActiveTimeTicks) ?
          Number(
              tabB.lastActiveTimeTicks.internalValue -
              tabA.lastActiveTimeTicks.internalValue) :
          0;
    });

    let mediaTabs: TabData[] = [];
    // Audio & Video section will not be added when search criteria is applied.
    // Show media tabs in Open Tabs.
    if (this.searchText_.length === 0) {
      mediaTabs = this.openTabs_.filter(
          tabData => tabHasMediaAlerts(tabData.tab as Tab));
    }

    const filteredMediaTabs =
        search<TabData>(this.searchText_, mediaTabs, this.searchOptions_);

    let filteredOpenTabs =
        search<TabData>(this.searchText_, this.openTabs_, this.searchOptions_);

    // The MRU tab that is not the active tab is either the first tab in the
    // Audio and Video section (if it exists) or the first tab in the Open Tabs
    // section.
    if (filteredOpenTabs.length > 0) {
      this.initiallySelectedIndex_ =
          (tabHasMediaAlerts(filteredOpenTabs[0]!.tab! as Tab) ||
           filteredMediaTabs.length === 0) ?
          1 :
          filteredMediaTabs.length + 2;
    }

    if (this.searchText_.length === 0) {
      filteredOpenTabs = filteredOpenTabs.filter(
          tabData => !tabHasMediaAlerts(tabData.tab as Tab));
    }

    this.filteredOpenTabsCount_ =
        filteredOpenTabs.length + filteredMediaTabs.length;

    const recentlyClosedItems: Array<TabData|TabGroupData> =
        [...this.recentlyClosedTabs_, ...this.recentlyClosedTabGroups_];
    recentlyClosedItems.sort((a, b) => {
      const aTime = this.getRecentlyClosedItemLastActiveTime_(a);
      const bTime = this.getRecentlyClosedItemLastActiveTime_(b);

      return (bTime && aTime) ?
          Number(bTime.internalValue - aTime.internalValue) :
          0;
    });
    let filteredRecentlyClosedItems = search<TabData|TabGroupData>(
        this.searchText_, recentlyClosedItems, this.searchOptions_);

    // Limit the number of recently closed items to the default display count
    // when no search text has been specified. Filter out recently closed tabs
    // that belong to a recently closed tab group by default.
    const recentlyClosedTabGroupIds = this.recentlyClosedTabGroups_.reduce(
        (acc, tabGroupData) => acc.concat(tabGroupData.tabGroup!.id),
        [] as Token[]);
    if (!this.searchText_.length) {
      filteredRecentlyClosedItems =
          filteredRecentlyClosedItems
              .filter(recentlyClosedItem => {
                if (recentlyClosedItem instanceof TabGroupData) {
                  return true;
                }

                const recentlyClosedTab =
                    (recentlyClosedItem as TabData).tab as RecentlyClosedTab;
                return (
                    !recentlyClosedTab.groupId ||
                    !recentlyClosedTabGroupIds.some(
                        groupId =>
                            tokenEquals(groupId, recentlyClosedTab.groupId!)));
              })
              .slice(0, this.recentlyClosedDefaultItemDisplayCount_);
    }

    this.filteredOpenHeaderIndices_ = [];
    let numItems = 0;
    [filteredMediaTabs, filteredOpenTabs, filteredRecentlyClosedItems].forEach(
        list => {
          if (list.length > 0) {
            this.filteredOpenHeaderIndices_.push(numItems);
            numItems += list.length + 1;
          }
        });

    this.filteredItems_ =
        ([
          [this.mediaTabsTitleItem_, filteredMediaTabs],
          [this.openTabsTitleItem_, filteredOpenTabs],
          [this.recentlyClosedTitleItem_, filteredRecentlyClosedItems],
        ] as Array<[TitleItem, Array<TabData|TabGroupData>]>)
            .reduce((acc, [sectionTitle, sectionItems]) => {
              if (sectionItems!.length !== 0) {
                acc.push(sectionTitle);
                if (!sectionTitle.expandable ||
                    sectionTitle.expandable && sectionTitle.expanded) {
                  acc.push(...sectionItems);
                }
              }
              return acc;
            }, [] as Array<TitleItem|TabData|TabGroupData>);
    this.searchResultText_ = this.getA11ySearchResultText_();

    // If there was no previously selected index, set the selected index to be
    // the tab index specified for initial selection; else retain the currently
    // selected index. If the list shrunk above the selected index, select the
    // last index in the list. If there are no matching results, set the
    // selected index value to none.
    await this.updateComplete;
    const tabsList = this.$.tabsList;
    await tabsList.updateComplete;
    // Only update the selection after the tab list has a chance to render
    // the newly filtered list.
    let selectedIndex = this.$.tabsList.selected;
    if (selectedIndex === NO_SELECTION) {
      selectedIndex = this.initiallySelectedIndex_;
    }
    tabsList.setSelected(
        Math.min(Math.max(selectedIndex, 0), this.lastSelectableIndex_()));
  }

  getSearchTextForTesting(): string {
    return this.searchText_;
  }

  getAvailableHeightForTesting(): number|undefined {
    return this.availableHeight_;
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onSelectedChanged_(
      e: CustomEvent<
          {item: (TabSearchItemElement | TabSearchGroupItemElement | null)}>) {
    const itemData = e.detail.item ? e.detail.item.data : null;
    this.activeSelectionId_ = (itemData && itemData instanceof TabData) ?
        itemData.tab.tabId.toString() :
        undefined;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-search-page': TabSearchPageElement;
  }
}

customElements.define(TabSearchPageElement.is, TabSearchPageElement);
