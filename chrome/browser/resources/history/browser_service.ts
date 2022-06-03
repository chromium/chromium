// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {RESULTS_PER_PAGE} from './constants.js';
import {ForeignSession, HistoryEntry, HistoryQuery} from './externs.js';

type RemoveVisitsRequest = Array<{
  url: string,
  timestamps: Array<number>,
}>;

/**
 * @fileoverview Defines a singleton object, history.BrowserService, which
 * provides access to chrome.send APIs.
 */

export class BrowserService {
  /** @return {!Promise<!Array<!ForeignSession>>} */
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
  removeVisits(removalList: RemoveVisitsRequest): Promise<void> {
    return sendWithPromise('removeVisits', removalList);
  }

  /** @param {string} sessionTag */
  openForeignSessionAllTabs(sessionTag: string) {
    chrome.send('openForeignSession', [sessionTag]);
  }

  openForeignSessionTab(
      sessionTag: string, windowId: number, tabId: number, e: MouseEvent) {
    chrome.send('openForeignSession', [
      sessionTag, String(windowId), String(tabId), e.button || 0, e.altKey,
      e.ctrlKey, e.metaKey, e.shiftKey
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

  navigateToUrl(url: string, target: string, e: MouseEvent) {
    chrome.send(
        'navigateToUrl',
        [url, target, e.button, e.altKey, e.ctrlKey, e.metaKey, e.shiftKey]);
  }

  otherDevicesInitialized() {
    chrome.send('otherDevicesInitialized');
  }

  /**
   * @return {!Promise<{info: !HistoryQuery, value: !Array<!HistoryEntry>}>}
   */
  queryHistoryContinuation() {
    return sendWithPromise('queryHistoryContinuation');
  }

  queryHistory(searchTerm: string):
      Promise<{info: HistoryQuery, value: Array<HistoryEntry>}> {
    return sendWithPromise('queryHistory', searchTerm, RESULTS_PER_PAGE);
  }

  startSignInFlow() {
    chrome.send('startSignInFlow');
  }

  static getInstance(): BrowserService {
    return instance || (instance = new BrowserService());
  }

  static setInstance(obj: BrowserService) {
    instance = obj;
  }
}

let instance: BrowserService|null = null;
