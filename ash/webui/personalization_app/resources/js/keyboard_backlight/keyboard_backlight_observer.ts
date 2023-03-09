// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {CurrentBacklightState, KeyboardBacklightObserverInterface, KeyboardBacklightObserverReceiver, KeyboardBacklightProviderInterface} from '../../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';

import {setCurrentBacklightStateAction, setWallpaperColorAction} from './keyboard_backlight_actions.js';
import {getKeyboardBacklightProvider} from './keyboard_backlight_interface_provider.js';

/** @fileoverview listens for updates on keyboard backlight settings changes. */

let instance: KeyboardBacklightObserver|null = null;

/**
 * Observes keyboard backlight changes and saves updates to
 * PersonalizationStore.
 */
export class KeyboardBacklightObserver implements
    KeyboardBacklightObserverInterface {
  static initKeyboardBacklightObserverIfNeeded(): void {
    if (!instance) {
      instance = new KeyboardBacklightObserver();
    }
  }

  static shutdown() {
    if (instance) {
      instance.receiver_.$.close();
      instance = null;
    }
  }

  private receiver_: KeyboardBacklightObserverReceiver =
      this.initReceiver_(getKeyboardBacklightProvider());

  private initReceiver_(keyboardBacklightProvider:
                            KeyboardBacklightProviderInterface):
      KeyboardBacklightObserverReceiver {
    const receiver = new KeyboardBacklightObserverReceiver(this);
    keyboardBacklightProvider.setKeyboardBacklightObserver(
        receiver.$.bindNewPipeAndPassRemote());
    return receiver;
  }

  onBacklightStateChanged(currentBacklightState: CurrentBacklightState): void {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setCurrentBacklightStateAction(currentBacklightState));
  }

  onWallpaperColorChanged(wallpaperColor: SkColor): void {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setWallpaperColorAction(wallpaperColor));
  }
}
