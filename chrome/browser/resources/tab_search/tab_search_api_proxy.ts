// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

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
  recordRecentlyClosedItemOpenAction(withSearch: boolean, isTab: boolean): void;

  redirectToNtp(): void;
}

export class TabSearchApiProxyImpl implements TabSearchApiProxy {
  recordRecentlyClosedItemOpenAction(withSearch: boolean, isTab: boolean) {
    chrome.metricsPrivate.recordEnumerationValue(
        isTab ? 'Tabs.TabSearch.WebUI.RecentlyClosedTabOpenAction' :
                'Tabs.TabSearch.WebUI.RecentlyClosedGroupOpenAction',
        withSearch ? RecentlyClosedItemOpenAction.WITH_SEARCH :
                     RecentlyClosedItemOpenAction.WITHOUT_SEARCH,
        RecentlyClosedItemOpenAction.COUNT);
  }

  redirectToNtp() {
    window.location.replace(loadTimeData.getString('newTabPageUrl'));
  }

  static getInstance(): TabSearchApiProxy {
    return instance || (instance = new TabSearchApiProxyImpl());
  }

  static setInstance(obj: TabSearchApiProxy) {
    instance = obj;
  }
}

let instance: TabSearchApiProxy|null = null;
