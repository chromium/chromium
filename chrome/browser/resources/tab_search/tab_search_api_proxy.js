// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote, ProfileData, SwitchToTabInfo} from './tab_search.mojom-webui.js';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const TabSwitchAction = {
  WITHOUT_SEARCH : 0,
  WITH_SEARCH : 1,
};

/** @interface */
export class TabSearchApiProxy {
  /**
   * @param {number} tabId
   * @param {boolean} withSearch
   * @param {number} closedTabIndex
   */
  closeTab(tabId, withSearch, closedTabIndex) {}

  /** @return {Promise<{profileData: ProfileData}>} */
  getProfileData() {}

  /** @param {number} tabId */
  openRecentlyClosedTab(tabId) {}

  /**
   * @param {!SwitchToTabInfo} info
   * @param {boolean} withSearch
   * @param {number} switchedTabIndex
   */
  switchToTab(info, withSearch, switchedTabIndex) {}

  /** @return {!PageCallbackRouter} */
  getCallbackRouter() {}

  showUI() {}

  closeUI() {}
}

/** @implements {TabSearchApiProxy} */
export class TabSearchApiProxyImpl {
  constructor() {
    /** @type {!PageCallbackRouter} */
    this.callbackRouter = new PageCallbackRouter();

    /** @type {!PageHandlerRemote} */
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  /** @override */
  closeTab(tabId, withSearch, closedTabIndex) {
    chrome.metricsPrivate.recordSmallCount(
        withSearch ? 'Tabs.TabSearch.WebUI.IndexOfCloseTabInFilteredList' :
                     'Tabs.TabSearch.WebUI.IndexOfCloseTabInUnfilteredList',
        closedTabIndex);
    this.handler.closeTab(tabId);
  }

  /** @override */
  getProfileData() {
    return this.handler.getProfileData();
  }

  /** @override */
  openRecentlyClosedTab(tabId) {
    this.handler.openRecentlyClosedTab(tabId);
  }

  /** @override */
  switchToTab(info, withSearch, switchedTabIndex) {
    chrome.metricsPrivate.recordEnumerationValue(
        'Tabs.TabSearch.WebUI.TabSwitchAction',
        withSearch ? TabSwitchAction.WITH_SEARCH
                   : TabSwitchAction.WITHOUT_SEARCH,
        Object.keys(TabSwitchAction).length);
    chrome.metricsPrivate.recordSmallCount(
        withSearch ? 'Tabs.TabSearch.WebUI.IndexOfSwitchTabInFilteredList' :
                     'Tabs.TabSearch.WebUI.IndexOfSwitchTabInUnfilteredList',
        switchedTabIndex);

    this.handler.switchToTab(info);
  }

  /** @override */
  getCallbackRouter() {
    return this.callbackRouter;
  }

  /** @override */
  showUI() {
    this.handler.showUI();
  }

  /** @override */
  closeUI() {
    this.handler.closeUI();
  }
}

addSingletonGetter(TabSearchApiProxyImpl);
