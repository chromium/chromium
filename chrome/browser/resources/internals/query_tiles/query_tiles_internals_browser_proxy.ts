// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface ServiceStatus {
  fetcherStatus: string;
  groupStatus: string;
}

export interface TileData {
  groupInfo: string;
  tilesProto: string;
}

let instance: QueryTilesInternalsBrowserProxy|null = null;

export interface QueryTilesInternalsBrowserProxy {
  /**
   * Start fetch right now.
   */
  startFetch(): void;

  /**
   * Delete everything in the database.
   */
  purgeDb(): void;

  /**
   * Get the current status of the TileService.
   * @return A promise firing when the service status is fetched.
   */
  getServiceStatus(): Promise<ServiceStatus>;

  /**
   * Get raw data from TileService database.
   * @return A promise firing when the raw data is fetched.
   */
  getTileData(): Promise<TileData>;

  /**
   * Set the base URL of query tile server.
   * @param url of the server.
   */
  setServerUrl(url: string): void;
}

export class QueryTilesInternalsBrowserProxyImpl implements
    QueryTilesInternalsBrowserProxy {
  startFetch() {
    chrome.send('startFetch');
  }

  purgeDb() {
    chrome.send('purgeDb');
  }

  getServiceStatus() {
    return sendWithPromise('getServiceStatus');
  }

  getTileData() {
    return sendWithPromise('getTileData');
  }

  setServerUrl(url: string) {
    chrome.send('setServerUrl', [url]);
  }

  static getInstance(): QueryTilesInternalsBrowserProxy {
    return instance || (instance = new QueryTilesInternalsBrowserProxyImpl());
  }
}
