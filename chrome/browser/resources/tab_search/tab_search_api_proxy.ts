// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ProfileData, SwitchToTabInfo, Tab, TabOrganizationModelStrategy, TabOrganizationSession, TabSearchSection, UserFeedback} from './tab_search.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './tab_search.mojom-webui.js';

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

  closeWebUiTab(): void;

  acceptTabOrganization(sessionId: number, organizationId: number, tabs: Tab[]):
      void;

  rejectTabOrganization(sessionId: number, organizationId: number): void;

  renameTabOrganization(
      sessionId: number, organizationId: number, name: string): void;

  getProfileData(): Promise<{profileData: ProfileData}>;

  getTabSearchSection(): Promise<{section: TabSearchSection}>;

  getTabOrganizationSession(): Promise<{session: TabOrganizationSession}>;

  getTabOrganizationModelStrategy():
      Promise<{strategy: TabOrganizationModelStrategy}>;

  getIsSplit(): Promise<{isSplit: boolean}>;

  openRecentlyClosedEntry(
      id: number, withSearch: boolean, isTab: boolean, index: number): void;

  requestTabOrganization(): void;

  rejectSession(sessionId: number): void;

  restartSession(): void;

  switchToTab(info: SwitchToTabInfo): void;

  getCallbackRouter(): PageCallbackRouter;

  removeTabFromOrganization(
      sessionId: number, organizationId: number, tab: Tab): void;

  replaceActiveSplitTab(replacementTabId: number): void;

  saveRecentlyClosedExpandedPref(expanded: boolean): void;

  startTabGroupTutorial(): void;

  triggerFeedback(sessionId: number): void;

  triggerSignIn(): void;

  openHelpPage(): void;

  setTabOrganizationModelStrategy(strategy: TabOrganizationModelStrategy): void;

  setTabOrganizationUserInstruction(user_instruction: string): void;

  setUserFeedback(sessionId: number, feedback: UserFeedback): void;

  notifyOrganizationUiReadyToShow(): void;

  notifySearchUiReadyToShow(): void;
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

  closeWebUiTab() {
    this.handler.closeWebUiTab();
  }

  acceptTabOrganization(
      sessionId: number, organizationId: number, tabs: Tab[]) {
    this.handler.acceptTabOrganization(sessionId, organizationId, tabs);
  }

  rejectTabOrganization(sessionId: number, organizationId: number) {
    this.handler.rejectTabOrganization(sessionId, organizationId);
  }

  renameTabOrganization(
      sessionId: number, organizationId: number, name: string) {
    this.handler.renameTabOrganization(sessionId, organizationId, name);
  }

  getProfileData() {
    return this.handler.getProfileData();
  }

  getTabSearchSection() {
    return this.handler.getTabSearchSection();
  }

  getTabOrganizationSession() {
    return this.handler.getTabOrganizationSession();
  }

  getTabOrganizationModelStrategy() {
    return this.handler.getTabOrganizationModelStrategy();
  }

  getIsSplit() {
    return this.handler.getIsSplit();
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

  rejectSession(sessionId: number) {
    this.handler.rejectSession(sessionId);
  }

  restartSession() {
    this.handler.restartSession();
  }

  switchToTab(info: SwitchToTabInfo) {
    this.handler.switchToTab(info);
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  removeTabFromOrganization(
      sessionId: number, organizationId: number, tab: Tab) {
    this.handler.removeTabFromOrganization(sessionId, organizationId, tab);
  }

  replaceActiveSplitTab(replacementSplitTabId: number) {
    this.handler.replaceActiveSplitTab(replacementSplitTabId);
  }

  saveRecentlyClosedExpandedPref(expanded: boolean) {
    this.handler.saveRecentlyClosedExpandedPref(expanded);
  }

  startTabGroupTutorial() {
    this.handler.startTabGroupTutorial();
  }

  triggerFeedback(sessionId: number) {
    this.handler.triggerFeedback(sessionId);
  }

  triggerSignIn() {
    this.handler.triggerSignIn();
  }

  openHelpPage() {
    this.handler.openHelpPage();
  }

  setTabOrganizationModelStrategy(strategy: TabOrganizationModelStrategy) {
    this.handler.setTabOrganizationModelStrategy(strategy);
  }

  setTabOrganizationUserInstruction(userInstruction: string) {
    this.handler.setTabOrganizationUserInstruction(userInstruction);
  }

  setUserFeedback(sessionId: number, feedback: UserFeedback) {
    this.handler.setUserFeedback(sessionId, feedback);
  }

  notifyOrganizationUiReadyToShow() {
    this.handler.notifyOrganizationUIReadyToShow();
  }

  notifySearchUiReadyToShow() {
    this.handler.notifySearchUIReadyToShow();
  }

  static getInstance(): TabSearchApiProxy {
    return instance || (instance = new TabSearchApiProxyImpl());
  }

  static setInstance(obj: TabSearchApiProxy) {
    instance = obj;
  }
}

let instance: TabSearchApiProxy|null = null;
