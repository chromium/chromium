// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import '/mojo/nearby_share_settings.mojom-lite.js';

/** @type {?nearbyShare.mojom.NearbyShareSettingsInterface} */
let nearbyShareSettings = null;

/** @type {boolean} */
let isTesting = false;

/**
 * @param {!nearbyShare.mojom.NearbyShareSettingsInterface}
 *     testNearbyShareSettings A test nearby share settings impl.
 */
export function setNearbyShareSettingsForTesting(testNearbyShareSettings) {
  nearbyShareSettings = testNearbyShareSettings;
  isTesting = true;
}

/**
 * @return {!nearbyShare.mojom.NearbyShareSettingsInterface}
 *     the Nearby Share settings interface
 */
export function getNearbyShareSettings() {
  if (!nearbyShareSettings) {
    nearbyShareSettings = nearbyShare.mojom.NearbyShareSettings.getRemote();
  }
  return nearbyShareSettings;
}

/**
 * @param {!nearbyShare.mojom.NearbyShareSettingsObserverInterface} observer
 * @return {?nearbyShare.mojom.NearbyShareSettingsObserverReceiver} the mojo
 *     receiver.
 */
export function observeNearbyShareSettings(observer) {
  if (isTesting) {
    getNearbyShareSettings().addSettingsObserver(
        /** @type {!nearbyShare.mojom.NearbyShareSettingsObserverRemote} */ (
            observer));
    return null;
  }

  const receiver =
      new nearbyShare.mojom.NearbyShareSettingsObserverReceiver(observer);
  getNearbyShareSettings().addSettingsObserver(
      receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
