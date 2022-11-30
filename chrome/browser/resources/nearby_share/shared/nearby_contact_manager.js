// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import '/mojo/nearby_share_settings.mojom-lite.js';

/** @type {?nearbyShare.mojom.ContactManagerInterface} */
let contactManager = null;
/** @type {boolean} */
let isTesting = false;
/**
 * @param {!nearbyShare.mojom.ContactManagerInterface}
 *     testContactManager A test contactManager impl.
 */
export function setContactManagerForTesting(testContactManager) {
  contactManager = testContactManager;
  isTesting = true;
}
/**
 * @return {!nearbyShare.mojom.ContactManagerInterface}
 *     the contactManager interface
 */
export function getContactManager() {
  if (!contactManager) {
    contactManager = nearbyShare.mojom.ContactManager.getRemote();
  }
  return contactManager;
}
/**
 * @param {!nearbyShare.mojom.DownloadContactsObserverInterface} observer
 * @return {?nearbyShare.mojom.DownloadContactsObserverReceiver} The mojo
 *     receiver or null when testing.
 */
export function observeContactManager(observer) {
  if (isTesting) {
    getContactManager().addDownloadContactsObserver(
        /** @type {!nearbyShare.mojom.DownloadContactsObserverRemote} */ (
            observer));
    return null;
  }
  const receiver =
      new nearbyShare.mojom.DownloadContactsObserverReceiver(observer);
  getContactManager().addDownloadContactsObserver(
      receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
