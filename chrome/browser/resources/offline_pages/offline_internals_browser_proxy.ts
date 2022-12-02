// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface OfflinePage {
  onlineUrl: string;
  creationTime: number;
  id: string;
  namespace: string;
  size: string;
  filePath: string;
  lastAccessTime: number;
  accessCount: number;
  isExpired: string;
  requestOrigin: string;
}

export interface SavePageRequest {
  status: string;
  onlineUrl: string;
  creationTime: number;
  id: string;
  namespace: string;
  lastAttemptTime: number;
  requestOrigin: string;
}

export interface IsLogging {
  modelIsLogging: boolean;
  queueIsLogging: boolean;
  prefetchIsLogging: boolean;
}

export interface OfflineInternalsBrowserProxy {
  /**
   * Gets current list of stored pages.
   * @return A promise firing when the list is fetched.
   */
  getStoredPages(): Promise<OfflinePage[]>;

  /**
   * Gets current offline queue requests.
   * @return A promise firing when the request queue is fetched.
   */
  getRequestQueue(): Promise<SavePageRequest[]>;

  /**
   * Deletes a set of pages from stored pages
   * @param ids A list of page IDs to delete.
   * @return A promise firing when the selected pages are deleted.
   */
  deleteSelectedPages(ids: string[]): Promise<string>;

  /**
   * Deletes a set of requests from the request queue
   * @param ids A list of request IDs to delete.
   * @return A promise firing when the selected pages are deleted.
   */
  deleteSelectedRequests(ids: string[]): Promise<string>;

  /**
   * Sets whether to record logs for stored pages.
   * @param shouldLog True if logging should be enabled.
   */
  setRecordPageModel(shouldLog: boolean): void;

  /**
   * Sets whether to record logs for scheduled requests.
   * @param shouldLog True if logging should be enabled.
   */
  setRecordRequestQueue(shouldLog: boolean): void;

  /**
   * Sets whether to record logs for prefetching.
   * @param shouldLog True if logging should be enabled.
   */
  setRecordPrefetchService(shouldLog: boolean): void;

  /**
   * Sets whether limitless prefetching is enabled.
   * @param enabled Whether to enable limitless prefetching.
   */
  setLimitlessPrefetchingEnabled(enabled: boolean): void;

  /**
   * Gets whether limitless prefetching is enabled.
   * @return Whether limitless prefetching is enabled
   */
  getLimitlessPrefetchingEnabled(): Promise<boolean>;

  /**
   * Sets the value to be sent with the prefetch testing header for
   * GeneratePageBundle requests.
   * @param value Value to send with X-Offline-Prefetch-Testing.
   */
  setPrefetchTestingHeaderValue(value: string): void;

  /**
   * Gets the value of the prefetch testing header to be sent with
   * GeneratePageBundle requests.
   * @return Header value.
   */
  getPrefetchTestingHeaderValue(): Promise<string>;

  /**
   * Gets the currently recorded logs.
   * @return A promise firing when the logs are retrieved.
   */
  getEventLogs(): Promise<string[]>;

  /**
   * Gets the state of logging (on/off).
   * @return A promise firing when the state is retrieved.
   */
  getLoggingState(): Promise<IsLogging>;

  /**
   * Adds the given url to the background loader queue.
   * @param url Url of the page to load later.
   * @return A promise firing after added to queue.
   *     Promise will return true if url has been successfully added.
   */
  addToRequestQueue(url: string): Promise<boolean>;

  /**
   * Gets the current network status in string form.
   * @return A promise firing when the network status is retrieved.
   */
  getNetworkStatus(): Promise<string>;

  /**
   * Schedules the default NWake task.  The returned Promise will reject if
   *     there is an error while scheduling.
   * @return A promise firing when the task has been scheduled.
   */
  scheduleNwake(): Promise<string>;

  /**
   * Cancels NWake task.
   * @return A promise firing when the task has been cancelled. The
   *     returned Promise will reject if there is an error.
   */
  cancelNwake(): Promise<string>;

  /**
   * Sends and processes a request to generate page bundle.
   * @param urls A list of comma-separated URLs.
   * @return A string describing the result.
   */
  generatePageBundle(urls: string): Promise<string>;

  /**
   * Sends and processes a request to get operation.
   * @param name Name of operation.
   * @return A string describing the result.
   */
  getOperation(name: string): Promise<string>;

  /**
   * Downloads an archive.
   * @param name Name of archive to download.
   */
  downloadArchive(name: string): void;
}

export class OfflineInternalsBrowserProxyImpl implements
    OfflineInternalsBrowserProxy {
  getStoredPages() {
    return sendWithPromise('getStoredPages');
  }

  getRequestQueue() {
    return sendWithPromise('getRequestQueue');
  }

  deleteSelectedPages(ids: string[]) {
    return sendWithPromise('deleteSelectedPages', ids);
  }

  deleteSelectedRequests(ids: string[]) {
    return sendWithPromise('deleteSelectedRequests', ids);
  }

  setRecordPageModel(shouldLog: boolean) {
    chrome.send('setRecordPageModel', [shouldLog]);
  }

  setRecordRequestQueue(shouldLog: boolean) {
    chrome.send('setRecordRequestQueue', [shouldLog]);
  }

  setRecordPrefetchService(shouldLog: boolean) {
    chrome.send('setRecordPrefetchService', [shouldLog]);
  }

  setLimitlessPrefetchingEnabled(enabled: boolean) {
    chrome.send('setLimitlessPrefetchingEnabled', [enabled]);
  }

  getLimitlessPrefetchingEnabled() {
    return sendWithPromise('getLimitlessPrefetchingEnabled');
  }

  setPrefetchTestingHeaderValue(value: string) {
    chrome.send('setPrefetchTestingHeader', [value]);
  }

  getPrefetchTestingHeaderValue() {
    return sendWithPromise('getPrefetchTestingHeader');
  }

  getEventLogs() {
    return sendWithPromise('getEventLogs');
  }

  getLoggingState() {
    return sendWithPromise('getLoggingState');
  }

  addToRequestQueue(url: string) {
    return sendWithPromise('addToRequestQueue', url);
  }

  getNetworkStatus() {
    return sendWithPromise('getNetworkStatus');
  }

  scheduleNwake() {
    return sendWithPromise('scheduleNwake');
  }

  cancelNwake() {
    return sendWithPromise('cancelNwake');
  }

  generatePageBundle(urls: string) {
    return sendWithPromise('generatePageBundle', urls);
  }

  getOperation(name: string) {
    return sendWithPromise('getOperation', name);
  }

  downloadArchive(name: string) {
    chrome.send('downloadArchive', [name]);
  }

  static getInstance(): OfflineInternalsBrowserProxy {
    return instance || (instance = new OfflineInternalsBrowserProxyImpl());
  }
}

let instance: OfflineInternalsBrowserProxy|null = null;
