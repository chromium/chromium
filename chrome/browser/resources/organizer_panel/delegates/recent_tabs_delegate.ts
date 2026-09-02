// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from '../organizer_list_section_delegate.js';
import type {OrganizerListSectionItem} from '../organizer_list_section_item.js';
import type {BrowserProxy, ProfileData, RecentlyClosedTab, TabsRemovedInfo} from '../tab_search.mojom-webui.js';
import {browserProxyFactory} from '../tab_search.mojom-webui.js';

export class RecentTabsDelegate implements
    OrganizerListSectionDelegate<RecentlyClosedTab> {
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();
  private client_?: OrganizerListSectionClient;
  private listenerIds_: number[] = [];
  private tabs_: RecentlyClosedTab[] = [];

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
      Promise<Array<OrganizerListSectionItem<RecentlyClosedTab>>> {
    await this.updateTabs_();
    return this.tabs_.map(tab => this.tabToSectionItem_(tab));
  }

  onItemClick(item: OrganizerListSectionItem<RecentlyClosedTab>) {
    const tab = item.data;
    assert(tab);
    this.browserProxy_.handler.openRecentlyClosedEntry(tab.tabId);
  }

  private async updateTabs_() {
    const {profileData} = await this.browserProxy_.handler.getProfileData();
    this.tabs_ = this.extractAndSortTabs_(profileData);
  }

  private notifyClient_() {
    this.client_?.onItemsChanged(
        this.tabs_.map(tab => this.tabToSectionItem_(tab)));
  }

  private onTabsChanged_(profileData: ProfileData) {
    this.tabs_ = this.extractAndSortTabs_(profileData);
    this.notifyClient_();
  }

  private onTabsRemoved_(tabsRemovedInfo: TabsRemovedInfo) {
    if (tabsRemovedInfo.recentlyClosedTabs.length === 0) {
      return;
    }

    const newTabIds =
        new Set(tabsRemovedInfo.recentlyClosedTabs.map(t => t.tabId));
    const allTabs = [
      ...tabsRemovedInfo.recentlyClosedTabs,
      ...this.tabs_.filter(t => !newTabIds.has(t.tabId)),
    ];

    this.tabs_ = this.sortTabs_(allTabs);
    this.notifyClient_();
  }

  private extractAndSortTabs_(profileData: ProfileData): RecentlyClosedTab[] {
    return this.sortTabs_([...profileData.recentlyClosedTabs]);
  }

  private sortTabs_(tabs: RecentlyClosedTab[]): RecentlyClosedTab[] {
    tabs.sort((a, b) => {
      const timeA = a.lastActiveTime?.internalValue ?? 0n;
      const timeB = b.lastActiveTime?.internalValue ?? 0n;
      return timeB > timeA ? 1 : (timeB < timeA ? -1 : 0);
    });

    return tabs;
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
