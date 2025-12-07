// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ContactManagerInterface, DownloadContactsObserverInterface, DownloadContactsObserverRemote} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {ContactManager, DownloadContactsObserverReceiver} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';

let contactManager: ContactManagerInterface|null = null;
let isTesting = false;

export function setContactManagerForTesting(
    testContactManager: ContactManagerInterface): void {
  contactManager = testContactManager;
  isTesting = true;
}

export function getContactManager(): ContactManagerInterface {
  if (!contactManager) {
    contactManager = ContactManager.getRemote();
  }
  return contactManager;
}

export function observeContactManager(
    observer: DownloadContactsObserverInterface):
    DownloadContactsObserverReceiver|null {
  if (isTesting) {
    getContactManager().addDownloadContactsObserver(
        observer as DownloadContactsObserverRemote);
    return null;
  }
  const receiver = new DownloadContactsObserverReceiver(observer);
  getContactManager().addDownloadContactsObserver(
      receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
