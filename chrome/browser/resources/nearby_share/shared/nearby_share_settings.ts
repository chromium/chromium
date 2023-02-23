// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NearbyShareSettings, NearbyShareSettingsInterface, NearbyShareSettingsObserverInterface, NearbyShareSettingsObserverReceiver, NearbyShareSettingsObserverRemote} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';

let nearbyShareSettings: NearbyShareSettingsInterface|null = null;
let isTesting = false;

export function setNearbyShareSettingsForTesting(
    testNearbyShareSettings: NearbyShareSettingsInterface): void {
  nearbyShareSettings = testNearbyShareSettings;
  isTesting = true;
}

export function getNearbyShareSettings(): NearbyShareSettingsInterface {
  if (!nearbyShareSettings) {
    nearbyShareSettings = NearbyShareSettings.getRemote();
  }
  return nearbyShareSettings;
}

export function observeNearbyShareSettings(
    observer: NearbyShareSettingsObserverInterface):
    NearbyShareSettingsObserverReceiver|null {
  if (isTesting) {
    getNearbyShareSettings().addSettingsObserver(
        observer as NearbyShareSettingsObserverRemote);
    return null;
  }

  const receiver = new NearbyShareSettingsObserverReceiver(observer);
  getNearbyShareSettings().addSettingsObserver(
      receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
