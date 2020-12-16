// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {RESULTS_PER_PAGE} from './constants.js';
import {ForeignSession, HistoryEntry, HistoryQuery} from './externs.js';

/**
 * @fileoverview Defines a singleton object, history.BrowserService, which
 * provides access to chrome.send APIs.
 */

export class BrowserService {
  /** @return {!Promise<!Array<!ForeignSession>>} */
  getForeignSessions() {
    return sendWithPromise('getForeignSessions');
  }

  /** @param {!string} url */
  removeBookmark(url) {
    chrome.send('removeBookmark', [url]);
  }

  /**
   * @param {!Array<!HistoryEntry>} removalList
   * @return {!Promise} Promise that is resolved when items are deleted
   *     successfully or rejected when deletion fails.
   */
  removeVisits(removalList) {
    return sendWithPromise('removeVisits', removalList);
  }

  /** @param {string} sessionTag */
  openForeignSessionAllTabs(sessionTag) {
    chrome.send('openForeignSession', [sessionTag]);
  }

  /**
   * @param {string} sessionTag
   * @param {number} windowId
   * @param {number} tabId
   * @param {MouseEvent} e
   */
  openForeignSessionTab(sessionTag, windowId, tabId, e) {
    chrome.send('openForeignSession', [
      sessionTag, String(windowId), String(tabId), e.button || 0, e.altKey,
      e.ctrlKey, e.metaKey, e.shiftKey
    ]);
  }

  /** @param {string} sessionTag */
  deleteForeignSession(sessionTag) {
    chrome.send('deleteForeignSession', [sessionTag]);
  }

  openClearBrowsingData() {
    chrome.send('clearBrowsingData');
  }

  /**
   * @param {string} histogram
   * @param {number} value
   * @param {number} max
   */
  recordHistogram(histogram, value, max) {
    chrome.send('metricsHandler:recordInHistogram', [histogram, value, max]);
  }

  /**
   * Record an action in UMA.
   * @param {string} action The name of the action to be logged.
   */
  recordAction(action) {
    if (action.indexOf('_') === -1) {
      action = `HistoryPage_${action}`;
    }
    chrome.send('metricsHandler:recordAction', [action]);
  }

  /**
   * @param {string} histogram
   * @param {number} time
   */
  recordTime(histogram, time) {
    chrome.send('metricsHandler:recordTime', [histogram, time]);
  }

  /**
   * @param {string} url
   * @param {string} target
   * @param {!MouseEvent} e
   */
  navigateToUrl(url, target, e) {
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

  /**
   * @param {string} searchTerm
   * @return {!Promise<{info: !HistoryQuery, value: !Array<!HistoryEntry>}>}
   */
  queryHistory(searchTerm) {
    return sendWithPromise('queryHistory', searchTerm, RESULTS_PER_PAGE);
  }

  startSignInFlow() {
    chrome.send('startSignInFlow');
  }
}

addSingletonGetter(BrowserService);
