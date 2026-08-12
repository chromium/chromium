// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ProfileData, SwitchToTabInfo, TokenRange} from './tab_search.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './tab_search.mojom-webui.js';

// LINT.IfChange(TabSearchRecentlyClosedItemOpenAction)
/**
 * These values are persisted to logs and should not be renumbered or reused.
 * See tools/metrics/histograms/metadata/tab/enums.xml.
 */
export enum RecentlyClosedItemOpenAction {
  WITHOUT_SEARCH = 0,
  WITH_SEARCH = 1,
  COUNT = WITH_SEARCH + 1,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:TabSearchRecentlyClosedItemOpenAction)

export interface TabSearchApiProxy {
  closeTab(tabId: number): void;

  closeTabs(tabIds: number[]): void;

  closeWebUiTab(): void;

  getProfileData(): Promise<{profileData: ProfileData}>;

  getIsSplit(): Promise<{isSplit: boolean}>;

  openRecentlyClosedEntry(id: number, withSearch: boolean, isTab: boolean):
      void;

  switchToTab(info: SwitchToTabInfo): void;

  getCallbackRouter(): PageCallbackRouter;

  replaceActiveSplitTab(replacementTabId: number): void;

  saveRecentlyClosedExpandedPref(expanded: boolean): void;

  maybeShowUi(): void;

  getRangesIgnoringCaseAndAccents(searchText: string, targets: string[]):
      Promise<{ranges: TokenRange[][]}>;
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

  closeTabs(tabIds: number[]) {
    this.handler.closeTabs(tabIds);
  }

  closeWebUiTab() {
    this.handler.closeWebUiTab();
  }

  getProfileData() {
    return this.handler.getProfileData();
  }

  getIsSplit() {
    return this.handler.getIsSplit();
  }

  openRecentlyClosedEntry(id: number, withSearch: boolean, isTab: boolean) {
    chrome.metricsPrivate.recordEnumerationValue(
        isTab ? 'Tabs.TabSearch.WebUI.RecentlyClosedTabOpenAction' :
                'Tabs.TabSearch.WebUI.RecentlyClosedGroupOpenAction',
        withSearch ? RecentlyClosedItemOpenAction.WITH_SEARCH :
                     RecentlyClosedItemOpenAction.WITHOUT_SEARCH,
        RecentlyClosedItemOpenAction.COUNT);
    this.handler.openRecentlyClosedEntry(id);
  }

  switchToTab(info: SwitchToTabInfo) {
    this.handler.switchToTab(info);
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  replaceActiveSplitTab(replacementSplitTabId: number) {
    this.handler.replaceActiveSplitTab(replacementSplitTabId);
  }

  saveRecentlyClosedExpandedPref(expanded: boolean) {
    this.handler.saveRecentlyClosedExpandedPref(expanded);
  }

  maybeShowUi() {
    this.handler.maybeShowUI();
  }

  getRangesIgnoringCaseAndAccents(searchText: string, targets: string[]) {
    return this.handler.getRangesIgnoringCaseAndAccents(searchText, targets);
  }

  static getInstance(): TabSearchApiProxy {
    return instance || (instance = new TabSearchApiProxyImpl());
  }

  static setInstance(obj: TabSearchApiProxy) {
    instance = obj;
  }
}

let instance: TabSearchApiProxy|null = null;
