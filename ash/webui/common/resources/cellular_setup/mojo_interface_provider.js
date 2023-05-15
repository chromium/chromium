// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CellularSetup, CellularSetupRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom-webui.js';
import {ESimManager, ESimManagerObserverInterface, ESimManagerObserverReceiver, ESimManagerObserverRemote, ESimManagerRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';

let cellularRemote = null;
let eSimManagerRemote = null;
let isTesting = false;

/**
 * @param {?CellularSetupRemote} testCellularRemote A test cellular remote
 */
export function setCellularSetupRemoteForTesting(testCellularRemote) {
  cellularRemote = testCellularRemote;
  isTesting = true;
}

/**
 * @returns {!CellularSetupRemote}
 */
export function getCellularSetupRemote() {
  if (cellularRemote) {
    return cellularRemote;
  }

  cellularRemote = CellularSetup.getRemote();
  return cellularRemote;
}

/**
 * @param {?ESimManagerRemote} testESimManagerRemote A test eSimManager remote
 */
export function setESimManagerRemoteForTesting(testESimManagerRemote) {
  eSimManagerRemote = testESimManagerRemote;
  isTesting = true;
}

/**
 * @returns {!ESimManagerRemote}
 */
export function getESimManagerRemote() {
  if (eSimManagerRemote) {
    return eSimManagerRemote;
  }

  eSimManagerRemote = ESimManager.getRemote();

  return eSimManagerRemote;
}

/**
 * @param {!ESimManagerObserverInterface} observer
 * @returns {?ESimManagerObserverReceiver}
 */
export function observeESimManager(observer) {
  if (isTesting) {
    getESimManagerRemote().addObserver(
        /** @type {!ESimManagerObserverRemote} */
        (observer));
    return null;
  }

  const receiver = new ESimManagerObserverReceiver(observer);
  getESimManagerRemote().addObserver(receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
