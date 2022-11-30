// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import '/mojo/nearby_share_target_types.mojom-lite.js';
import '/mojo/nearby_share_share_type.mojom-lite.js';
import '/mojo/nearby_share.mojom-lite.js';
import '/mojo/nearby_share_settings.mojom-lite.js';

/** @type {?nearbyShare.mojom.ReceiveManagerInterface} */
let receiveManager = null;
/** @type {boolean} */
let isTesting = false;

/**
 * @param {!nearbyShare.mojom.ReceiveManagerInterface}
 *     testReceiveManager A test receiveManager impl.
 */
export function setReceiveManagerForTesting(testReceiveManager) {
  receiveManager = testReceiveManager;
  isTesting = true;
}

/**
 * @return {!nearbyShare.mojom.ReceiveManagerInterface}
 *     the receiveManager interface
 */
export function getReceiveManager() {
  if (!receiveManager) {
    receiveManager = nearbyShare.mojom.ReceiveManager.getRemote();
  }
  return receiveManager;
}

/**
 * @param {!nearbyShare.mojom.ReceiveObserverInterface} observer
 * @return {?nearbyShare.mojom.ReceiveObserverReceiver} The mojo
 *     receiver or null when testing.
 */
export function observeReceiveManager(observer) {
  if (isTesting) {
    getReceiveManager().addReceiveObserver(
        /** @type {!nearbyShare.mojom.ReceiveObserverRemote} */ (observer));
    return null;
  }

  const receiver = new nearbyShare.mojom.ReceiveObserverReceiver(observer);
  getReceiveManager().addReceiveObserver(receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
