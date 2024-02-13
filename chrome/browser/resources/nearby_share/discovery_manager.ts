// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides methods to get or set an instance of a
 * DiscoveryManager which allows interaction with native code.
 */

import type {DiscoveryManagerInterface, DiscoveryManagerRemote, DiscoveryObserverInterface, DiscoveryObserverRemote} from '/shared/nearby_share.mojom-webui.js';
import {DiscoveryManager, DiscoveryObserverReceiver} from '/shared/nearby_share.mojom-webui.js';

let discoveryManager: DiscoveryManagerInterface|null = null;
let isTesting: boolean = false;

export function setDiscoveryManagerForTesting(
    testDiscoveryManager: DiscoveryManagerInterface) {
  discoveryManager = testDiscoveryManager;
  isTesting = true;
}

export function getDiscoveryManager(): DiscoveryManagerInterface {
  if (discoveryManager) {
    return discoveryManager;
  }

  discoveryManager = DiscoveryManager.getRemote();
  (discoveryManager as DiscoveryManagerRemote)
      .onConnectionError.addListener(() => discoveryManager = null);
  return discoveryManager;
}

/**
 * @return The mojo receiver or null when testing.
 */
export function observeDiscoveryManager(observer: DiscoveryObserverInterface):
    DiscoveryObserverReceiver|null {
  if (isTesting) {
    getDiscoveryManager().addDiscoveryObserver(
        observer as DiscoveryObserverRemote);
    return null;
  }

  const receiver = new DiscoveryObserverReceiver(observer);
  getDiscoveryManager().addDiscoveryObserver(
      receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
