// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides methods to get or set an instance of a
 * DiscoveryManager which allows interaction with native code.
 */

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import './nearby_share_target_types.mojom-lite.js';
import './nearby_share.mojom-lite.js';

/** @type {?nearbyShare.mojom.DiscoveryManagerInterface} */
let discoveryManager = null;

/**
 * @param {!nearbyShare.mojom.DiscoveryManagerInterface}
 *     testDiscoveryManager A test discovery manager.
 */
export function setDiscoveryManagerForTesting(testDiscoveryManager) {
  discoveryManager = testDiscoveryManager;
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
