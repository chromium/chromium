// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides methods to get or set an instance of a
 * DiscoveryManager which allows interaction with native code.
 */

import {DiscoveryManager, DiscoveryManagerInterface, DiscoveryObserverInterface, DiscoveryObserverReceiver, DiscoveryObserverRemote} from '/mojo/nearby_share.mojom-webui.js';

/** @type {?DiscoveryManagerInterface} */
let discoveryManager = null;
/** @type {boolean} */
let isTesting = false;

/**
 * @param {!DiscoveryManagerInterface} testDiscoveryManager
 */
export function setDiscoveryManagerForTesting(testDiscoveryManager) {
  discoveryManager = testDiscoveryManager;
  isTesting = true;
}

/**
 * @return {!DiscoveryManagerInterface} Discovery manager.
 */
export function getDiscoveryManager() {
  if (discoveryManager) {
    return discoveryManager;
  }

  discoveryManager = DiscoveryManager.getRemote();
  discoveryManager.onConnectionError.addListener(() => discoveryManager = null);
  return discoveryManager;
}

/**
 * @param {!DiscoveryObserverInterface} observer
 * @return {?DiscoveryObserverReceiver} The mojo receiver or null when testing.
 */
export function observeDiscoveryManager(observer) {
  if (isTesting) {
    getDiscoveryManager().addDiscoveryObserver(
        /** @type {!DiscoveryObserverRemote} */ (observer));
    return null;
  }

  const receiver = new DiscoveryObserverReceiver(observer);
  getDiscoveryManager().addDiscoveryObserver(
      receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
