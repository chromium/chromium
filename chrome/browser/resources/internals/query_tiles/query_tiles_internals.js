// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {QueryTilesInternalsBrowserProxy, QueryTilesInternalsBrowserProxyImpl, ServiceStatus, TileData} from './query_tiles_internals_browser_proxy.js';

/**
 * @param {!ServiceStatus} serviceStatus The current status of the tile
 *     service.
 */
function onServiceStatusChanged(serviceStatus) {
  $('group-status').textContent = serviceStatus.groupStatus;
  $('fetcher-status').textContent = serviceStatus.fetcherStatus;
}

/**
 * @param {!TileData} tileData The raw data persisted in database.
 */
function onTileDataAvailable(tileData) {
  $('group-info').textContent = tileData.groupInfo;
  $('tile-proto').textContent = tileData.tilesProto;
}

function initialize() {
  /** @type {!QueryTilesInternalsBrowserProxy} */
  const browserProxy = QueryTilesInternalsBrowserProxyImpl.getInstance();

  // Register all event listeners.
  addWebUIListener('service-status-changed', onServiceStatusChanged);

  addWebUIListener('tile-data-available', onTileDataAvailable);

  $('start-fetch').onclick = function() {
    browserProxy.startFetch();
  };

  $('purge-db').onclick = function() {
    browserProxy.purgeDb();
  };

  $('prototype-server').onclick = function() {
    $('base-url').value =
        'https://staging-gsaprototype-pa.sandbox.googleapis.com';
  };

  $('prod-server').onclick = function() {
    $('base-url').value = 'https://chromeupboarding-pa.googleapis.com';
  };

  $('set-url').onclick = function() {
    browserProxy.setServerUrl($('base-url').value);
  };
  // Kick off requests for the current system state.
  browserProxy.getServiceStatus().then(onServiceStatusChanged);
  browserProxy.getTileData().then(onTileDataAvailable);
}

document.addEventListener('DOMContentLoaded', initialize);
