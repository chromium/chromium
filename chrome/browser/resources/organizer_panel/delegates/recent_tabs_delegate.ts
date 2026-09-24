// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from '../organizer_list_section_delegate.js';
import type {OrganizerListSectionItem, OrganizerListSectionItemDescriptionPart} from '../organizer_list_section_item.js';
import type {BrowserProxy, ProfileData, RecentlyClosedTab, RecentlyClosedTabGroup, TabsRemovedInfo} from '../tab_search.mojom-webui.js';
import {browserProxyFactory} from '../tab_search.mojom-webui.js';

import {compareTimeDescending, getTabDescriptionParts, tokenToString} from './tab_delegate_utils.js';

// Union type to represent all possible recently closed items (tabs, tab groups,
// split views).
export type RecentlyClosedItem = RecentlyClosedTab|RecentlyClosedTabGroup;

function isTabGroup(item: RecentlyClosedItem): item is RecentlyClosedTabGroup {
  return 'sessionId' in item;
}

export class RecentTabsDelegate implements
    OrganizerListSectionDelegate<RecentlyClosedItem> {
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();
  private client_?: OrganizerListSectionClient;
  private listenerIds_: number[] = [];
  private tabs_: RecentlyClosedTab[] = [];
  private tabGroups_: RecentlyClosedTabGroup[] = [];
  private items_: RecentlyClosedItem[] = [];

  init(sectionClient: OrganizerListSectionClient) {
    this.client_ = sectionClient;
    const callbackRouter = this.browserProxy_.callbackRouter;
    this.listenerIds_.push(
        callbackRouter.tabsChanged.addListener((profileData: ProfileData) => {
          this.onTabsChanged_(profileData);
        }),
        callbackRouter.tabsRemoved.addListener(
            (tabsRemovedInfo: TabsRemovedInfo) => {
              this.onTabsRemoved_(tabsRemovedInfo);
            }));
  }

  getHeader(): string {
    return loadTimeData.getString('recentlyClosed');
  }

  async getItems():
      Promise<Array<OrganizerListSectionItem<RecentlyClosedItem>>> {
    await this.updateItems_();
    return this.items_.map(item => this.toSectionItem_(item));
  }

  onItemClick(item: OrganizerListSectionItem<RecentlyClosedItem>) {
    const data = item.data;
    assert(data);
    const id = isTabGroup(data) ? data.sessionId : data.tabId;
    this.browserProxy_.handler.openRecentlyClosedEntry(id);
  }

  private async updateItems_() {
    const {profileData} = await this.browserProxy_.handler.getProfileData();
    this.tabs_ = profileData.recentlyClosedTabs;
    this.tabGroups_ = profileData.recentlyClosedTabGroups;
    this.items_ = this.extractAndSortItems_();
  }

  private notifyClient_() {
    this.client_?.onItemsChanged(
        this.items_.map(item => this.toSectionItem_(item)));
  }

  private onTabsChanged_(profileData: ProfileData) {
    this.tabs_ = profileData.recentlyClosedTabs;
    this.tabGroups_ = profileData.recentlyClosedTabGroups;
    this.items_ = this.extractAndSortItems_();
    this.notifyClient_();
  }

  private onTabsRemoved_(tabsRemovedInfo: TabsRemovedInfo) {
    if (tabsRemovedInfo.recentlyClosedTabs.length === 0) {
      return;
    }

    const newTabIds =
        new Set(tabsRemovedInfo.recentlyClosedTabs.map(t => t.tabId));
    this.tabs_ = [
      ...tabsRemovedInfo.recentlyClosedTabs,
      ...this.tabs_.filter(t => !newTabIds.has(t.tabId)),
    ];

    this.items_ = this.extractAndSortItems_();
    this.notifyClient_();
  }

  private extractAndSortItems_(): RecentlyClosedItem[] {
    const groupIds =
        new Set(this.tabGroups_.map(group => tokenToString(group.id)));
    const filteredTabs = this.tabs_.filter(
        tab => !tab.groupId || !groupIds.has(tokenToString(tab.groupId)));

    const allItems: RecentlyClosedItem[] =
        [...filteredTabs, ...this.tabGroups_];
    return this.sortItems_(allItems);
  }

  private sortItems_(items: RecentlyClosedItem[]): RecentlyClosedItem[] {
    items.sort(
        (a, b) => compareTimeDescending(a.lastActiveTime, b.lastActiveTime));

    return items;
  }

  private toSectionItem_(item: RecentlyClosedItem):
      OrganizerListSectionItem<RecentlyClosedItem> {
    return isTabGroup(item) ? this.tabGroupToSectionItem_(item) :
                              this.tabToSectionItem_(item);
  }

  private tabToSectionItem_(tab: RecentlyClosedTab):
      OrganizerListSectionItem<RecentlyClosedItem> {
    return {
      title: [tab.title],
      description: getTabDescriptionParts([tab.url], tab.lastActiveElapsedText),
      prefixIcon: {
        url: tab.url,
      },
      data: tab,
    };
  }

  private tabGroupToSectionItem_(tabGroup: RecentlyClosedTabGroup):
      OrganizerListSectionItem<RecentlyClosedItem> {
    const description: OrganizerListSectionItemDescriptionPart[] = [];
    const tabCount = tabGroup.tabCount;
    description.push({
      text: loadTimeData.getStringF(
          tabCount === 1 ? 'oneTab' : 'tabCount', tabCount),
    });

    if (tabGroup.lastActiveElapsedText) {
      description.push({
        text: tabGroup.lastActiveElapsedText,
      });
    }

    return {
      title: [tabGroup.title],
      description,
      data: tabGroup,
    };
  }
}
