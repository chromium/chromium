// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ReceiveManager, ReceiveManagerInterface, ReceiveObserverInterface, ReceiveObserverReceiver, ReceiveObserverRemote} from '/mojo/nearby_share.mojom-webui.js';

/** @type {?ReceiveManagerInterface} */
let receiveManager = null;
/** @type {boolean} */
let isTesting = false;

/**
 * @param {!ReceiveManagerInterface} testReceiveManager
 */
export function setReceiveManagerForTesting(testReceiveManager) {
  receiveManager = testReceiveManager;
  isTesting = true;
}

/**
 * @return {!ReceiveManagerInterface} the receiveManager interface
 */
export function getReceiveManager() {
  if (!receiveManager) {
    receiveManager = ReceiveManager.getRemote();
  }
  return receiveManager;
}

/**
 * @param {!ReceiveObserverInterface} observer
 * @return {?ReceiveObserverReceiver} The mojo receiver or null when testing.
 */
export function observeReceiveManager(observer) {
  if (isTesting) {
    getReceiveManager().addReceiveObserver(
        /** @type {!ReceiveObserverRemote} */ (observer));
    return null;
  }

  const receiver = new ReceiveObserverReceiver(observer);
  getReceiveManager().addReceiveObserver(receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
