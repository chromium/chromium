// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from '../organizer_list_section_delegate.js';
import type {OrganizerListSectionItem} from '../organizer_list_section_item.js';
import type {BrowserProxy, ProfileData, RecentlyClosedTab, TabsRemovedInfo} from '../tab_search.mojom-webui.js';
import {browserProxyFactory} from '../tab_search.mojom-webui.js';

// Union type to represent all possible recently closed items (tabs, tab groups,
// split views).
export type RecentlyClosedItem = RecentlyClosedTab;

export class RecentTabsDelegate implements
    OrganizerListSectionDelegate<RecentlyClosedItem> {
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();
  private client_?: OrganizerListSectionClient;
  private listenerIds_: number[] = [];
  private tabs_: RecentlyClosedTab[] = [];
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
    this.browserProxy_.handler.openRecentlyClosedEntry(data.tabId);
  }

  private async updateItems_() {
    const {profileData} = await this.browserProxy_.handler.getProfileData();
    this.tabs_ = profileData.recentlyClosedTabs;
    this.items_ = this.extractAndSortItems_();
  }

  private notifyClient_() {
    this.client_?.onItemsChanged(
        this.items_.map(item => this.toSectionItem_(item)));
  }

  private onTabsChanged_(profileData: ProfileData) {
    this.tabs_ = profileData.recentlyClosedTabs;
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
    return this.sortItems_(this.tabs_);
  }

  private sortItems_(items: RecentlyClosedItem[]): RecentlyClosedItem[] {
    items.sort((a, b) => {
      const timeA = a.lastActiveTime?.internalValue ?? 0n;
      const timeB = b.lastActiveTime?.internalValue ?? 0n;
      return timeB > timeA ? 1 : (timeB < timeA ? -1 : 0);
    });

    return items;
  }

  private toSectionItem_(item: RecentlyClosedItem):
      OrganizerListSectionItem<RecentlyClosedItem> {
    return this.tabToSectionItem_(item);
  }

  private tabToSectionItem_(tab: RecentlyClosedTab):
      OrganizerListSectionItem<RecentlyClosedTab> {
    const description: string[] = [];
    try {
      const url = new URL(tab.url);
      description.push(url.hostname);
    } catch {
      description.push(tab.url);
    }

    if (tab.lastActiveElapsedText) {
      description.push(tab.lastActiveElapsedText);
    }

    return {
      title: tab.title,
      description,
      prefixIcon: {
        urls: [tab.url],
      },
      data: tab,
    };
  }
}
