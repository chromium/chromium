// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides methods to get or set an instance of a
 * DiscoveryManager which allows interaction with native code.
 */

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import './mojo/nearby_share_target_types.mojom-lite.js';
import './mojo/nearby_share_share_type.mojom-lite.js';
import './mojo/nearby_share.mojom-lite.js';

/** @type {?nearbyShare.mojom.DiscoveryManagerInterface} */
let discoveryManager = null;
/** @type {boolean} */
let isTesting = false;

/**
 * @param {!nearbyShare.mojom.DiscoveryManagerInterface}
 *     testDiscoveryManager A test discovery manager.
 */
export function setDiscoveryManagerForTesting(testDiscoveryManager) {
  discoveryManager = testDiscoveryManager;
  isTesting = true;
}

/**
 * @return {!nearbyShare.mojom.DiscoveryManagerInterface} Discovery manager.
 */
export function getDiscoveryManager() {
  if (discoveryManager) {
    return discoveryManager;
  }

  discoveryManager = nearbyShare.mojom.DiscoveryManager.getRemote();
  discoveryManager.onConnectionError.addListener(() => discoveryManager = null);
  return discoveryManager;
}

/**
 * @param {!nearbyShare.mojom.DiscoveryObserverInterface} observer
 * @return {?nearbyShare.mojom.DiscoveryObserverReceiver} The mojo
 *     receiver or null when testing.
 */
export function observeDiscoveryManager(observer) {
  if (isTesting) {
    getDiscoveryManager().addDiscoveryObserver(
        /** @type {!nearbyShare.mojom.DiscoveryObserverRemote} */ (observer));
    return null;
  }

  const receiver = new nearbyShare.mojom.DiscoveryObserverReceiver(observer);
  getDiscoveryManager().addDiscoveryObserver(
      receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
