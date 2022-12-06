// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {QueryTilesInternalsBrowserProxy, QueryTilesInternalsBrowserProxyImpl, ServiceStatus, TileData} from './query_tiles_internals_browser_proxy.js';

/**
 * @param serviceStatus The current status of the tile service.
 */
function onServiceStatusChanged(serviceStatus: ServiceStatus) {
  getRequiredElement('group-status').textContent = serviceStatus.groupStatus;
  getRequiredElement('fetcher-status').textContent =
      serviceStatus.fetcherStatus;
}

/**
 * @param tileData The raw data persisted in database.
 */
function onTileDataAvailable(tileData: TileData) {
  getRequiredElement('group-info').textContent = tileData.groupInfo;
  getRequiredElement('tile-proto').textContent = tileData.tilesProto;
}

function initialize() {
  const browserProxy: QueryTilesInternalsBrowserProxy =
      QueryTilesInternalsBrowserProxyImpl.getInstance();

  // Register all event listeners.
  addWebUiListener('service-status-changed', onServiceStatusChanged);

  addWebUiListener('tile-data-available', onTileDataAvailable);

  getRequiredElement('start-fetch').onclick = function() {
    browserProxy.startFetch();
  };

  getRequiredElement('purge-db').onclick = function() {
    browserProxy.purgeDb();
  };

  getRequiredElement('prototype-server').onclick = function() {
    getRequiredElement<HTMLInputElement>('base-url').value =
        'https://staging-gsaprototype-pa.sandbox.googleapis.com';
  };

  getRequiredElement('prod-server').onclick = function() {
    getRequiredElement<HTMLInputElement>('base-url').value =
        'https://chromeupboarding-pa.googleapis.com';
  };

  getRequiredElement('set-url').onclick = function() {
    browserProxy.setServerUrl(
        getRequiredElement<HTMLInputElement>('base-url').value);
  };
  // Kick off requests for the current system state.
  browserProxy.getServiceStatus().then(onServiceStatusChanged);
  browserProxy.getTileData().then(onTileDataAvailable);
}

document.addEventListener('DOMContentLoaded', initialize);
