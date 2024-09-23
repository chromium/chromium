// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CellularSetup, CellularSetupInterface} from '//resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom-webui.js';
import {ESimManager, ESimManagerInterface, ESimManagerObserverInterface, ESimManagerObserverReceiver, ESimManagerObserverRemote} from '//resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';

let cellularRemote: CellularSetupInterface|null = null;
let eSimManagerRemote: ESimManagerInterface|null = null;
let isTesting: boolean = false;

export function setCellularSetupRemoteForTesting(
    testCellularRemote: CellularSetupInterface|null): void {
  cellularRemote = testCellularRemote;
  isTesting = true;
}

export function getCellularSetupRemote(): CellularSetupInterface {
  if (cellularRemote) {
    return cellularRemote;
  }

  cellularRemote = CellularSetup.getRemote();
  return cellularRemote;
}

export function setESimManagerRemoteForTesting(
    testESimManagerRemote: ESimManagerInterface|null): void {
  eSimManagerRemote = testESimManagerRemote;
  isTesting = true;
}

export function getESimManagerRemote(): ESimManagerInterface {
  if (eSimManagerRemote) {
    return eSimManagerRemote;
  }

  eSimManagerRemote = ESimManager.getRemote();

  return eSimManagerRemote;
}

export function observeESimManager(observer: ESimManagerObserverInterface):
    ESimManagerObserverReceiver|null {
  if (isTesting) {
    getESimManagerRemote().addObserver(observer as ESimManagerObserverRemote);
    return null;
  }

  const receiver = new ESimManagerObserverReceiver(observer);
  getESimManagerRemote().addObserver(receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
