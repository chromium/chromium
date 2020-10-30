// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import './tab_search.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

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
  /** @param {number} tabId */
  closeTab(tabId) {}

  /** @return {Promise<{profileTabs: tabSearch.mojom.ProfileTabs}>} */
  getProfileTabs() {}

  showFeedbackPage() {}

  /**
   * @param {!tabSearch.mojom.SwitchToTabInfo} info
   * @param {boolean} withSearch
   */
  switchToTab(info, withSearch) {}

  /** @return {!tabSearch.mojom.PageCallbackRouter} */
  getCallbackRouter() {}

  showUI() {}

  closeUI() {}
}

/** @implements {TabSearchApiProxy} */
export class TabSearchApiProxyImpl {
  constructor() {
    /** @type {!tabSearch.mojom.PageCallbackRouter} */
    this.callbackRouter = new tabSearch.mojom.PageCallbackRouter();

    /** @type {!tabSearch.mojom.PageHandlerRemote} */
    this.handler = new tabSearch.mojom.PageHandlerRemote();

    const factory = tabSearch.mojom.PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  /** @override */
  closeTab(tabId) {
    this.handler.closeTab(tabId);
  }

  /** @override */
  getProfileTabs() {
    return this.handler.getProfileTabs();
  }

  /** @override */
  showFeedbackPage() {
    this.handler.showFeedbackPage();
  }

  /** @override */
  switchToTab(info, withSearch) {
    chrome.metricsPrivate.recordEnumerationValue(
        'Tabs.TabSearch.WebUI.TabSwitchAction',
        withSearch ? TabSwitchAction.WITH_SEARCH
                   : TabSwitchAction.WITHOUT_SEARCH,
        Object.keys(TabSwitchAction).length);
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
