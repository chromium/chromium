// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * @typedef {{
 *   onlineUrl: string,
 *   creationTime: number,
 *   id: string,
 *   namespace: string,
 *   size: string,
 *   filePath: string,
 *   lastAccessTime: number,
 *   accessCount: number,
 *   isExpired: string,
 *   requestOrigin: string
 * }}
 */
export let OfflinePage;

/**
 * @typedef {{
 *   status: string,
 *   onlineUrl: string,
 *   creationTime: number,
 *   id: string,
 *   namespace: string,
 *   lastAttemptTime: number,
 *   requestOrigin: string
 * }}
 */
export let SavePageRequest;

/**
 * @typedef {{
 *   modelIsLogging: boolean,
 *   queueIsLogging: boolean,
 *   prefetchIsLogging: boolean
 * }}
 */
export let IsLogging;

/** @interface */
export class OfflineInternalsBrowserProxy {
  /**
   * Gets current list of stored pages.
   * @return {!Promise<!Array<OfflinePage>>} A promise firing when the
   *     list is fetched.
   */
  getStoredPages() {}

  /**
   * Gets current offline queue requests.
   * @return {!Promise<!Array<SavePageRequest>>} A promise firing when the
   *     request queue is fetched.
   */
  getRequestQueue() {}

  /**
   * Deletes a set of pages from stored pages
   * @param {!Array<string>} ids A list of page IDs to delete.
   * @return {!Promise<!string>} A promise firing when the selected
   *     pages are deleted.
   */
  deleteSelectedPages(ids) {}

  /**
   * Deletes a set of requests from the request queue
   * @param {!Array<string>} ids A list of request IDs to delete.
   * @return {!Promise<!string>} A promise firing when the selected
   *     pages are deleted.
   */
  deleteSelectedRequests(ids) {}

  /**
   * Sets whether to record logs for stored pages.
   * @param {boolean} shouldLog True if logging should be enabled.
   */
  setRecordPageModel(shouldLog) {}

  /**
   * Sets whether to record logs for scheduled requests.
   * @param {boolean} shouldLog True if logging should be enabled.
   */
  setRecordRequestQueue(shouldLog) {}

  /**
   * Sets whether to record logs for prefetching.
   * @param {boolean} shouldLog True if logging should be enabled.
   */
  setRecordPrefetchService(shouldLog) {}

  /**
   * Sets whether limitless prefetching is enabled.
   * @param {boolean} enabled Whether to enable limitless prefetching.
   */
  setLimitlessPrefetchingEnabled(enabled) {}

  /**
   * Gets whether limitless prefetching is enabled.
   * @return {!Promise<boolean>} Whether limitless prefetching is enabled
   */
  getLimitlessPrefetchingEnabled() {}

  /**
   * Sets the value to be sent with the prefetch testing header for
   * GeneratePageBundle requests.
   * @param {string} value Value to send with X-Offline-Prefetch-Testing.
   */
  setPrefetchTestingHeaderValue(value) {}

  /**
   * Gets the value of the prefetch testing header to be sent with
   * GeneratePageBundle requests.
   * @return {!Promise<string>} Header value.
   */
  getPrefetchTestingHeaderValue() {}

  /**
   * Gets the currently recorded logs.
   * @return {!Promise<!Array<string>>} A promise firing when the
   *     logs are retrieved.
   */
  getEventLogs() {}

  /**
   * Gets the state of logging (on/off).
   * @return {!Promise<!IsLogging>} A promise firing when the state
   *     is retrieved.
   */
  getLoggingState() {}

  /**
   * Adds the given url to the background loader queue.
   * @param {string} url Url of the page to load later.
   * @return {!Promise<boolean>} A promise firing after added to queue.
   *     Promise will return true if url has been successfully added.
   */
  addToRequestQueue(url) {}

  /**
   * Gets the current network status in string form.
   * @return {!Promise<string>} A promise firing when the network status
   *     is retrieved.
   */
  getNetworkStatus() {}

  /**
   * Schedules the default NWake task.  The returned Promise will reject if
   *     there is an error while scheduling.
   * @return {!Promise<string>} A promise firing when the task has been
   *     scheduled.
   */
  scheduleNwake() {}

  /**
   * Cancels NWake task.
   * @return {!Promise} A promise firing when the task has been cancelled. The
   *     returned Promise will reject if there is an error.
   */
  cancelNwake() {}

  /**
   * Shows the prefetching notification with an example origin.
   * @return {!Promise<string>} A promise firing when the notification has
   *   been shown.
   */
  showPrefetchNotification() {}

  /**
   * Sends and processes a request to generate page bundle.
   * @param {string} urls A list of comma-separated URLs.
   * @return {!Promise<string>} A string describing the result.
   */
  generatePageBundle(urls) {}

  /**
   * Sends and processes a request to get operation.
   * @param {string} name Name of operation.
   * @return {!Promise<string>} A string describing the result.
   */
  getOperation(name) {}

  /**
   * Downloads an archive.
   * @param {string} name Name of archive to download.
   */
  downloadArchive(name) {}
}

/** @implements {OfflineInternalsBrowserProxy} */
export class OfflineInternalsBrowserProxyImpl {
  /** @override */
  getStoredPages() {
    return sendWithPromise('getStoredPages');
  }

  /** @override */
  getRequestQueue() {
    return sendWithPromise('getRequestQueue');
  }

  /** @override */
  deleteSelectedPages(ids) {
    return sendWithPromise('deleteSelectedPages', ids);
  }

  /** @override */
  deleteSelectedRequests(ids) {
    return sendWithPromise('deleteSelectedRequests', ids);
  }

  /** @override */
  setRecordPageModel(shouldLog) {
    chrome.send('setRecordPageModel', [shouldLog]);
  }

  /** @override */
  setRecordRequestQueue(shouldLog) {
    chrome.send('setRecordRequestQueue', [shouldLog]);
  }

  /** @override */
  setRecordPrefetchService(shouldLog) {
    chrome.send('setRecordPrefetchService', [shouldLog]);
  }

  /** @override */
  setLimitlessPrefetchingEnabled(enabled) {
    chrome.send('setLimitlessPrefetchingEnabled', [enabled]);
  }

  /** @override */
  getLimitlessPrefetchingEnabled() {
    return sendWithPromise('getLimitlessPrefetchingEnabled');
  }

  /** @override */
  setPrefetchTestingHeaderValue(value) {
    chrome.send('setPrefetchTestingHeader', [value]);
  }

  /** @override */
  getPrefetchTestingHeaderValue() {
    return sendWithPromise('getPrefetchTestingHeader');
  }

  /** @override */
  getEventLogs() {
    return sendWithPromise('getEventLogs');
  }

  /** @override */
  getLoggingState() {
    return sendWithPromise('getLoggingState');
  }

  /** @override */
  addToRequestQueue(url) {
    return sendWithPromise('addToRequestQueue', url);
  }

  /** @override */
  getNetworkStatus() {
    return sendWithPromise('getNetworkStatus');
  }

  /** @override */
  scheduleNwake() {
    return sendWithPromise('scheduleNwake');
  }

  /** @override */
  cancelNwake() {
    return sendWithPromise('cancelNwake');
  }

  /** @override */
  showPrefetchNotification() {
    return sendWithPromise('showPrefetchNotification');
  }

  /** @override */
  generatePageBundle(urls) {
    return sendWithPromise('generatePageBundle', urls);
  }

  /** @override */
  getOperation(name) {
    return sendWithPromise('getOperation', name);
  }

  /** @override */
  downloadArchive(name) {
    chrome.send('downloadArchive', [name]);
  }
}

addSingletonGetter(OfflineInternalsBrowserProxyImpl);
