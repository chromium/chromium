// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from '../organizer_list_section_delegate.js';
import type {OrganizerListSectionItem} from '../organizer_list_section_item.js';
import type {BrowserProxy, ProfileData, Tab, TabsRemovedInfo, TabUpdateInfo} from '../tab_search.mojom-webui.js';
import {browserProxyFactory} from '../tab_search.mojom-webui.js';

export class OpenTabsDelegate implements OrganizerListSectionDelegate<Tab> {
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
    // TODO(b/549784710): Use localized string.
    return 'Open Tabs';
  }

  async getItems(): Promise<Array<OrganizerListSectionItem<Tab>>> {
    await this.updateTabs_();
    return this.tabs_.map(tab => this.tabToSectionItem_(tab));
  }

  onItemClick(item: OrganizerListSectionItem<Tab>) {
    const tab = item.data;
    if (tab) {
      this.browserProxy_.handler.switchToTab({tabId: tab.tabId});
    }
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

  private extractAndSortTabs_(profileData: ProfileData): Tab[] {
    const allTabs: Tab[] = [];
    for (const window of profileData.windows) {
      for (const tab of window.tabs) {
        allTabs.push(tab);
      }
    }

    allTabs.sort((a, b) => {
      const timeA = a.lastActiveTimeTicks?.internalValue ?? 0n;
      const timeB = b.lastActiveTimeTicks?.internalValue ?? 0n;
      return timeB > timeA ? 1 : (timeB < timeA ? -1 : 0);
    });

    return allTabs;
  }

  private tabToSectionItem_(tab: Tab): OrganizerListSectionItem<Tab> {
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
