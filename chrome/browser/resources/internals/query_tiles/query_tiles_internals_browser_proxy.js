// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * @typedef {{
 *   fetcherStatus: string,
 *   groupStatus: string,
 * }}
 */
export let ServiceStatus;

/**
 * @typedef {{
 *   groupInfo: string,
 *   tilesProto: string,
 * }}
 */
export let TileData;

/** @interface */
export class QueryTilesInternalsBrowserProxy {
  /**
   * Start fetch right now.
   */
  startFetch() {}

  /**
   * Delete everything in the database.
   */
  purgeDb() {}

  /**
   * Get the current status of the TileService.
   * @return {!Promise<ServiceStatus>} A promise firing when the service
   *     status is fetched.
   */
  getServiceStatus() {}

  /**
   * Get raw data from TileService database.
   * @return {!Promise<TileData>} A promise firing when the raw data
   * is fetched.
   */
  getTileData() {}

  /**
   * Set the base URL of query tile server.
   * @param {string} url of the server.
   */
  setServerUrl(url) {}
}

/**
 * @implements {QueryTilesInternalsBrowserProxy}
 */
export class QueryTilesInternalsBrowserProxyImpl {
  /** @override */
  startFetch() {
    return chrome.send('startFetch');
  }

  /** @override */
  purgeDb() {
    return chrome.send('purgeDb');
  }

  /** @override */
  getServiceStatus() {
    return sendWithPromise('getServiceStatus');
  }

  /** @override */
  getTileData() {
    return sendWithPromise('getTileData');
  }

  /** @override */
  setServerUrl(url) {
    chrome.send('setServerUrl', [url]);
  }
}

addSingletonGetter(QueryTilesInternalsBrowserProxyImpl);
