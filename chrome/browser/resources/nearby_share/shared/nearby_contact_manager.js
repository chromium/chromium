// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ContactManager, ContactManagerInterface, DownloadContactsObserverInterface, DownloadContactsObserverReceiver, DownloadContactsObserverRemote} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';

/** @type {?ContactManagerInterface} */
let contactManager = null;
/** @type {boolean} */
let isTesting = false;
/**
 * @param {!ContactManagerInterface} testContactManager A test impl.
 */
export function setContactManagerForTesting(testContactManager) {
  contactManager = testContactManager;
  isTesting = true;
}
/**
 * @return {!ContactManagerInterface} the contactManager interface
 */
export function getContactManager() {
  if (!contactManager) {
    contactManager = ContactManager.getRemote();
  }
  return contactManager;
}
/**
 * @param {!DownloadContactsObserverInterface} observer
 * @return {?DownloadContactsObserverReceiver} The mojo receiver or null
 *   when testing.
 */
export function observeContactManager(observer) {
  if (isTesting) {
    getContactManager().addDownloadContactsObserver(
        /** @type {!DownloadContactsObserverRemote} */ (observer));
    return null;
  }
  const receiver = new DownloadContactsObserverReceiver(observer);
  getContactManager().addDownloadContactsObserver(
      receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
