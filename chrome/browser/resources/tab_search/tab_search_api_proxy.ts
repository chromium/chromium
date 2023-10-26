// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote, ProfileData, SwitchToTabInfo, Tab, TabOrganizationSession} from './tab_search.mojom-webui.js';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 */
export enum RecentlyClosedItemOpenAction {
  WITHOUT_SEARCH = 0,
  WITH_SEARCH = 1,
}

export interface TabSearchApiProxy {
  closeTab(tabId: number): void;

  acceptTabOrganization(
      sessionId: number, organizationId: number, name: string,
      tabs: Tab[]): void;

  rejectTabOrganization(sessionId: number, organizationId: number): void;

  getProfileData(): Promise<{profileData: ProfileData}>;

  getTabOrganizationSession(): Promise<{session: TabOrganizationSession}>;

  openRecentlyClosedEntry(
      id: number, withSearch: boolean, isTab: boolean, index: number): void;

  requestTabOrganization(): void;

  switchToTab(info: SwitchToTabInfo): void;

  getCallbackRouter(): PageCallbackRouter;

  saveRecentlyClosedExpandedPref(expanded: boolean): void;

  setTabIndex(index: number): void;

  startTabGroupTutorial(): void;

  showUi(): void;
}

export class TabSearchApiProxyImpl implements TabSearchApiProxy {
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  handler: PageHandlerRemote = new PageHandlerRemote();

  constructor() {
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  closeTab(tabId: number) {
    this.handler.closeTab(tabId);
  }

  acceptTabOrganization(
      sessionId: number, organizationId: number, name: string, tabs: Tab[]) {
    this.handler.acceptTabOrganization(sessionId, organizationId, name, tabs);
  }

  rejectTabOrganization(sessionId: number, organizationId: number) {
    this.handler.rejectTabOrganization(sessionId, organizationId);
  }

  getProfileData() {
    return this.handler.getProfileData();
  }

  getTabOrganizationSession() {
    return this.handler.getTabOrganizationSession();
  }

  openRecentlyClosedEntry(
      id: number, withSearch: boolean, isTab: boolean, index: number) {
    chrome.metricsPrivate.recordEnumerationValue(
        isTab ? 'Tabs.TabSearch.WebUI.RecentlyClosedTabOpenAction' :
                'Tabs.TabSearch.WebUI.RecentlyClosedGroupOpenAction',
        withSearch ? RecentlyClosedItemOpenAction.WITH_SEARCH :
                     RecentlyClosedItemOpenAction.WITHOUT_SEARCH,
        Object.keys(RecentlyClosedItemOpenAction).length);
    chrome.metricsPrivate.recordSmallCount(
        withSearch ?
            'Tabs.TabSearch.WebUI.IndexOfOpenRecentlyClosedEntryInFilteredList' :
            'Tabs.TabSearch.WebUI.IndexOfOpenRecentlyClosedEntryInUnfilteredList',
        index);
    this.handler.openRecentlyClosedEntry(id);
  }

  requestTabOrganization() {
    this.handler.requestTabOrganization();
  }

  switchToTab(info: SwitchToTabInfo) {
    this.handler.switchToTab(info);
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  saveRecentlyClosedExpandedPref(expanded: boolean) {
    this.handler.saveRecentlyClosedExpandedPref(expanded);
  }

  setTabIndex(index: number) {
    this.handler.setTabIndex(index);
  }

  startTabGroupTutorial() {
    this.handler.startTabGroupTutorial();
  }

  showUi() {
    this.handler.showUI();
  }

  static getInstance(): TabSearchApiProxy {
    return instance || (instance = new TabSearchApiProxyImpl());
  }

  static setInstance(obj: TabSearchApiProxy) {
    instance = obj;
  }
}

let instance: TabSearchApiProxy|null = null;
