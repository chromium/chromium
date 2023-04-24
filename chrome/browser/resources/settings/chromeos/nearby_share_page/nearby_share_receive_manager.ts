// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ReceiveManager, ReceiveManagerInterface, ReceiveObserverInterface, ReceiveObserverReceiver, ReceiveObserverRemote} from '/shared/nearby_share.mojom-webui.js';

let receiveManager: ReceiveManagerInterface|null = null;
let isTesting = false;

export function setReceiveManagerForTesting(
    testReceiveManager: ReceiveManagerInterface): void {
  receiveManager = testReceiveManager;
  isTesting = true;
}

export function getReceiveManager(): ReceiveManagerInterface {
  if (!receiveManager) {
    receiveManager = ReceiveManager.getRemote();
  }
  return receiveManager;
}

export function observeReceiveManager(observer: ReceiveObserverInterface):
    ReceiveObserverReceiver|null {
  if (isTesting) {
    getReceiveManager().addReceiveObserver(observer as ReceiveObserverRemote);
    return null;
  }

  const receiver = new ReceiveObserverReceiver(observer);
  getReceiveManager().addReceiveObserver(receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
