// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {Token} from '//resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';

import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from '../organizer_list_section_delegate.js';
import type {OrganizerListSectionItem, OrganizerListSectionItemIcon} from '../organizer_list_section_item.js';
import type {BrowserProxy, ProfileData, Tab, TabsRemovedInfo, TabUpdateInfo} from '../tab_search.mojom-webui.js';
import {browserProxyFactory, SplitTabLayout} from '../tab_search.mojom-webui.js';

export enum OpenTabsItemType {
  TAB = 'tab',
  SPLIT_TAB = 'split_tab',
}

interface SingleTabItem {
  type: OpenTabsItemType.TAB;
  tab: Tab;
}

interface SplitTabItem {
  type: OpenTabsItemType.SPLIT_TAB;
  tabs: [Tab, Tab];
}

export type OpenTabsItem = SingleTabItem|SplitTabItem;

export function isSplitTab(item: OpenTabsItem): item is SplitTabItem {
  return item.type === OpenTabsItemType.SPLIT_TAB;
}

function tokenToString(token: Token): string {
  return `${token.high.toString()}#${token.low.toString()}`;
}

function getHostnameOrUrl(url: string): string {
  try {
    return new URL(url).hostname;
  } catch {
    return url;
  }
}

function getLastActiveTimeTicks(tab: Tab): bigint {
  return tab.lastActiveTimeTicks?.internalValue ?? 0n;
}

function getMostRecentTab(item: OpenTabsItem): Tab {
  if (!isSplitTab(item)) {
    return item.tab;
  }
  return getLastActiveTimeTicks(item.tabs[1]) >
          getLastActiveTimeTicks(item.tabs[0]) ?
      item.tabs[1] :
      item.tabs[0];
}

export class OpenTabsDelegate implements
    OrganizerListSectionDelegate<OpenTabsItem> {
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();
  private client_?: OrganizerListSectionClient;
  private listenerIds_: number[] = [];
  private tabs_: Tab[] = [];

  init(sectionClient: OrganizerListSectionClient) {
    this.client_ = sectionClient;
    const callbackRouter = this.browserProxy_.callbackRouter;
    this.listenerIds_.push(
        callbackRouter.tabsChanged.addListener((profileData: ProfileData) => {
          this.onTabsChanged_(profileData);
        }),
        callbackRouter.tabUpdated.addListener(
            (tabUpdateInfo: TabUpdateInfo) => {
              this.onTabUpdated_(tabUpdateInfo);
            }),
        callbackRouter.tabsRemoved.addListener(
            (tabsRemovedInfo: TabsRemovedInfo) => {
              this.onTabsRemoved_(tabsRemovedInfo);
            }));
  }

  getHeader(): string {
    return loadTimeData.getString('openTabs');
  }

  async getItems(): Promise<Array<OrganizerListSectionItem<OpenTabsItem>>> {
    await this.updateTabs_();
    return this.getOpenTabsItems_().map(item => this.toSectionItem_(item));
  }

  onItemClick(item: OrganizerListSectionItem<OpenTabsItem>) {
    const data = item.data;
    assert(data);
    const tabId = isSplitTab(data) ? data.tabs[0].tabId : data.tab.tabId;
    this.browserProxy_.handler.switchToTab({tabId});
  }

  onItemActionButtonClicked(item: OrganizerListSectionItem<OpenTabsItem>) {
    const data = item.data;
    assert(data);
    if (isSplitTab(data)) {
      this.browserProxy_.handler.closeTabs(
          [data.tabs[0].tabId, data.tabs[1].tabId]);
    } else {
      this.browserProxy_.handler.closeTab(data.tab.tabId);
    }
  }

  private async updateTabs_() {
    const {profileData} = await this.browserProxy_.handler.getProfileData();
    this.tabs_ = this.extractTabs_(profileData);
  }

  private notifyClient_() {
    this.client_?.onItemsChanged(
        this.getOpenTabsItems_().map(item => this.toSectionItem_(item)));
  }

  private onTabsChanged_(profileData: ProfileData) {
    this.tabs_ = this.extractTabs_(profileData);
    this.notifyClient_();
  }

  private async onTabUpdated_(tabUpdateInfo: TabUpdateInfo) {
    const updatedTab = tabUpdateInfo.tab;
    const index = this.tabs_.findIndex(t => t.tabId === updatedTab.tabId);
    if (index !== -1) {
      this.tabs_[index] = updatedTab;
    } else {
      await this.updateTabs_();
    }
    this.notifyClient_();
  }

  private onTabsRemoved_(tabsRemovedInfo: TabsRemovedInfo) {
    const removedIds = new Set(tabsRemovedInfo.tabIds);
    this.tabs_ = this.tabs_.filter(t => !removedIds.has(t.tabId));
    this.notifyClient_();
  }

  private extractTabs_(profileData: ProfileData): Tab[] {
    const allTabs: Tab[] = [];
    for (const window of profileData.windows) {
      for (const tab of window.tabs) {
        allTabs.push(tab);
      }
    }
    return allTabs;
  }

  private getOpenTabsItems_(): OpenTabsItem[] {
    const items: OpenTabsItem[] = [];
    const splitTabsMap = new Map<string, Tab[]>();
    const nonSplitTabs: Tab[] = [];

    for (const tab of this.tabs_) {
      if (tab.splitId) {
        const splitIdStr = tokenToString(tab.splitId);
        if (!splitTabsMap.has(splitIdStr)) {
          splitTabsMap.set(splitIdStr, []);
        }
        splitTabsMap.get(splitIdStr)!.push(tab);
      } else {
        nonSplitTabs.push(tab);
      }
    }

    for (const tabs of splitTabsMap.values()) {
      if (tabs.length === 2) {
        items.push({
          type: OpenTabsItemType.SPLIT_TAB,
          tabs: [tabs[0]!, tabs[1]!],
        });
      } else {
        // If one tab of a split view was removed via `onTabsRemoved_`, the
        // remaining tab may still have a `splitId` until the browser clears it
        // in a subsequent update. Render any unmatched tab as a single tab.
        for (const tab of tabs) {
          items.push({
            type: OpenTabsItemType.TAB,
            tab,
          });
        }
      }
    }

    for (const tab of nonSplitTabs) {
      items.push({
        type: OpenTabsItemType.TAB,
        tab,
      });
    }

    items.sort((a, b) => {
      const timeA = getLastActiveTimeTicks(getMostRecentTab(a));
      const timeB = getLastActiveTimeTicks(getMostRecentTab(b));
      return timeB > timeA ? 1 : (timeB < timeA ? -1 : 0);
    });

    return items;
  }

  private toSectionItem_(item: OpenTabsItem):
      OrganizerListSectionItem<OpenTabsItem> {
    let title: string;
    let tabs: Tab[];
    let prefixIcon: OrganizerListSectionItemIcon;

    if (isSplitTab(item)) {
      title = loadTimeData.getString('splitView');
      tabs = item.tabs;
      prefixIcon = {
        stackedFavicons: {
          urls: [item.tabs[0].url, item.tabs[1].url],
          stackVertically: item.tabs[0].splitLayout === SplitTabLayout.kStacked,
        },
      };
    } else {
      title = item.tab.title;
      tabs = [item.tab];
      prefixIcon = {url: item.tab.url};
    }

    const description = tabs.map(tab => getHostnameOrUrl(tab.url));
    const elapsedText = getMostRecentTab(item).lastActiveElapsedText;
    if (elapsedText) {
      description.push(elapsedText);
    }

    return {
      title,
      description,
      prefixIcon,
      hoveredActionButton: {
        icon: 'cr:close',
        ariaLabel: loadTimeData.getString('closeTab'),
      },
      data: item,
    };
  }
}
