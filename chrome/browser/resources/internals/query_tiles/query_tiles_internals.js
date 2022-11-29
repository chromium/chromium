// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';

import {QueryTilesInternalsBrowserProxy, QueryTilesInternalsBrowserProxyImpl, ServiceStatus, TileData} from './query_tiles_internals_browser_proxy.js';

/**
 * @param {!ServiceStatus} serviceStatus The current status of the tile
 *     service.
 */
function onServiceStatusChanged(serviceStatus) {
  document.body.querySelector('#group-status').textContent =
      serviceStatus.groupStatus;
  document.body.querySelector('#fetcher-status').textContent =
      serviceStatus.fetcherStatus;
}

/**
 * @param {!TileData} tileData The raw data persisted in database.
 */
function onTileDataAvailable(tileData) {
  document.body.querySelector('#group-info').textContent = tileData.groupInfo;
  document.body.querySelector('#tile-proto').textContent = tileData.tilesProto;
}

function initialize() {
  /** @type {!QueryTilesInternalsBrowserProxy} */
  const browserProxy = QueryTilesInternalsBrowserProxyImpl.getInstance();

  // Register all event listeners.
  addWebUIListener('service-status-changed', onServiceStatusChanged);

  addWebUIListener('tile-data-available', onTileDataAvailable);

  document.body.querySelector('#start-fetch').onclick = function() {
    browserProxy.startFetch();
  };

  document.body.querySelector('#purge-db').onclick = function() {
    browserProxy.purgeDb();
  };

  document.body.querySelector('#prototype-server').onclick = function() {
    document.body.querySelector('#base-url').value =
        'https://staging-gsaprototype-pa.sandbox.googleapis.com';
  };

  document.body.querySelector('#prod-server').onclick = function() {
    document.body.querySelector('#base-url').value =
        'https://chromeupboarding-pa.googleapis.com';
  };

  document.body.querySelector('#set-url').onclick = function() {
    browserProxy.setServerUrl(document.body.querySelector('#base-url').value);
  };
  // Kick off requests for the current system state.
  browserProxy.getServiceStatus().then(onServiceStatusChanged);
  browserProxy.getTileData().then(onTileDataAvailable);
}

document.addEventListener('DOMContentLoaded', initialize);
