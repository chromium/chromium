// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

import {RESULTS_PER_PAGE} from './constants.js';
import type {ForeignSession, HistoryEntry, HistoryQuery} from './externs.js';

export type RemoveVisitsRequest = Array<{
  url: string,
  timestamps: number[],
}>;

export interface QueryResult {
  info: HistoryQuery;
  value: HistoryEntry[];
}

/**
 * @fileoverview Defines a singleton object, history.BrowserService, which
 * provides access to chrome.send APIs.
 */

export interface BrowserService {
  getForeignSessions(): Promise<ForeignSession[]>;
  removeBookmark(url: string): void;
  removeVisits(removalList: RemoveVisitsRequest): Promise<void>;
  setLastSelectedTab(lastSelectedTab: number): void;
  openForeignSessionAllTabs(sessionTag: string): void;
  openForeignSessionTab(sessionTag: string, tabId: number, e: MouseEvent): void;
  deleteForeignSession(sessionTag: string): void;
  openClearBrowsingData(): void;
  recordHistogram(histogram: string, value: number, max: number): void;
  recordAction(action: string): void;
  recordTime(histogram: string, time: number): void;
  recordLongTime(histogram: string, time: number): void;
  navigateToUrl(url: string, target: string, e: MouseEvent): void;
  otherDevicesInitialized(): void;
  queryHistoryContinuation(): Promise<QueryResult>;
  queryHistory(searchTerm: string, beginTime?: number): Promise<QueryResult>;
  startTurnOnSyncFlow(): void;
}

export class BrowserServiceImpl implements BrowserService {
  getForeignSessions() {
    return sendWithPromise('getForeignSessions');
  }

  removeBookmark(url: string) {
    chrome.send('removeBookmark', [url]);
  }

  /**
   * @return Promise that is resolved when items are deleted
   *     successfully or rejected when deletion fails.
   */
  removeVisits(removalList: RemoveVisitsRequest) {
    return sendWithPromise('removeVisits', removalList);
  }

  setLastSelectedTab(lastSelectedTab: number) {
    chrome.send('setLastSelectedTab', [lastSelectedTab]);
  }

  openForeignSessionAllTabs(sessionTag: string) {
    chrome.send('openForeignSessionAllTabs', [sessionTag]);
  }

  openForeignSessionTab(sessionTag: string, tabId: number, e: MouseEvent) {
    chrome.send('openForeignSessionTab', [
      sessionTag,
      String(tabId),
      e.button || 0,
      e.altKey,
      e.ctrlKey,
      e.metaKey,
      e.shiftKey,
    ]);
  }

  deleteForeignSession(sessionTag: string) {
    chrome.send('deleteForeignSession', [sessionTag]);
  }

  openClearBrowsingData() {
    chrome.send('clearBrowsingData');
  }

  recordHistogram(histogram: string, value: number, max: number) {
    chrome.send('metricsHandler:recordInHistogram', [histogram, value, max]);
  }

  /**
   * Record an action in UMA.
   * @param action The name of the action to be logged.
   */
  recordAction(action: string) {
    if (action.indexOf('_') === -1) {
      action = `HistoryPage_${action}`;
    }
    chrome.send('metricsHandler:recordAction', [action]);
  }

  recordTime(histogram: string, time: number) {
    chrome.send('metricsHandler:recordTime', [histogram, time]);
  }

  recordLongTime(histogram: string, time: number) {
    // It's a bit odd that this is the only one to use chrome.metricsPrivate,
    // but that's because the other code predates chrome.metricsPrivate.
    // In any case, the MetricsHandler doesn't support long time histograms.
    chrome.metricsPrivate.recordLongTime(histogram, time);
  }

  navigateToUrl(url: string, target: string, e: MouseEvent) {
    chrome.send(
        'navigateToUrl',
        [url, target, e.button, e.altKey, e.ctrlKey, e.metaKey, e.shiftKey]);
  }

  otherDevicesInitialized() {
    chrome.send('otherDevicesInitialized');
  }

  queryHistoryContinuation() {
    return sendWithPromise('queryHistoryContinuation');
  }

  queryHistory(searchTerm: string, beginTime?: number) {
    return sendWithPromise(
        'queryHistory', searchTerm, RESULTS_PER_PAGE, beginTime);
  }

  startTurnOnSyncFlow() {
    chrome.send('startTurnOnSyncFlow');
  }

  static getInstance(): BrowserService {
    return instance || (instance = new BrowserServiceImpl());
  }

  static setInstance(obj: BrowserService) {
    instance = obj;
  }
}

let instance: BrowserService|null = null;
