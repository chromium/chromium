// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NearbyShareSettings, NearbyShareSettingsInterface, NearbyShareSettingsObserverInterface, NearbyShareSettingsObserverReceiver, NearbyShareSettingsObserverRemote} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';

/** @type {?NearbyShareSettingsInterface} */
let nearbyShareSettings = null;

/** @type {boolean} */
let isTesting = false;

/**
 * @param {!NearbyShareSettingsInterface} testNearbyShareSettings
 */
export function setNearbyShareSettingsForTesting(testNearbyShareSettings) {
  nearbyShareSettings = testNearbyShareSettings;
  isTesting = true;
}

/**
 * @return {!NearbyShareSettingsInterface} the Nearby Share settings interface
 */
export function getNearbyShareSettings() {
  if (!nearbyShareSettings) {
    nearbyShareSettings = NearbyShareSettings.getRemote();
  }
  return nearbyShareSettings;
}

/**
 * @param {!NearbyShareSettingsObserverInterface} observer
 * @return {?NearbyShareSettingsObserverReceiver} the mojo receiver.
 */
export function observeNearbyShareSettings(observer) {
  if (isTesting) {
    getNearbyShareSettings().addSettingsObserver(
        /** @type {!NearbyShareSettingsObserverRemote} */ (observer));
    return null;
  }

  const receiver = new NearbyShareSettingsObserverReceiver(observer);
  getNearbyShareSettings().addSettingsObserver(
      receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
