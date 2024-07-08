// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FullscreenPreviewState} from 'chrome://resources/ash/common/personalization/wallpaper_state.js';

import {CurrentAttribution, CurrentWallpaper, WallpaperObserverInterface, WallpaperObserverReceiver, WallpaperProviderInterface, WallpaperType} from '../../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';

import {setAttributionAction, setFullscreenStateAction, setSelectedImageAction, setUpdatedDailyRefreshImageAction} from './wallpaper_actions.js';
import {getDailyRefreshState} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

let instance: WallpaperObserver|null = null;
let initialLoadTimeout: number|null = null;
const setTimeout = window.setTimeout;
const clearTimeout = window.clearTimeout;

/**
 * Set up the observer to listen for wallpaper changes.
 */
function initWallpaperObserver(
    wallpaperProvider: WallpaperProviderInterface,
    target: WallpaperObserverInterface): WallpaperObserverReceiver {
  const receiver = new WallpaperObserverReceiver(target);
  wallpaperProvider.setWallpaperObserver(receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}

/**
 * @classdesc Implements interface |WallpaperObserver| generated from
 * |personalization_app.mojom|. See comments there for method descriptions.
 */
export class WallpaperObserver implements WallpaperObserverInterface {
  /**
   * Create a new wallpaper observer instance if no instance currently running.
   */
  static initWallpaperObserverIfNeeded(): void {
    if (!instance) {
      instance = new WallpaperObserver();
      initialLoadTimeout = setTimeout(() => {
        const store = PersonalizationStore.getInstance();
        // If still loading the initial currently selected wallpaper image after
        // 120 seconds, consider this an error and update the store.
        store.dispatch(setSelectedImageAction(null));
        initialLoadTimeout = null;
      }, 120 * 1000);
    }
  }

  private receiver_: WallpaperObserverReceiver =
      initWallpaperObserver(getWallpaperProvider(), this);

  onWallpaperPreviewEnded() {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setFullscreenStateAction(FullscreenPreviewState.OFF));
  }

  onAttributionChanged(attribution: CurrentAttribution|null) {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setAttributionAction(attribution));
  }

  onWallpaperChanged(currentWallpaper: CurrentWallpaper|null) {
    const store = PersonalizationStore.getInstance();

    if (store.data.wallpaper.fullscreen === FullscreenPreviewState.LOADING) {
      // Ignore current wallpaper updates while in fullscreen preview mode. The
      // attribution information is for the old (non-preview) wallpaper. This is
      // because setting an image in preview mode updates the image but not the
      // stored WallpaperInfo. The wallpaper app should treat the duration of
      // preview mode as loading. Another onWallpaperChanged will fire when
      // preview mode is canceled or confirmed.
      store.dispatch(setFullscreenStateAction(FullscreenPreviewState.VISIBLE));
      return;
    }
    if (initialLoadTimeout) {
      clearTimeout(initialLoadTimeout);
      initialLoadTimeout = null;
    }

    store.dispatch(setSelectedImageAction(currentWallpaper));

    if (currentWallpaper &&
        (currentWallpaper.type == WallpaperType.kDailyGooglePhotos ||
         currentWallpaper.type == WallpaperType.kDaily ||
         currentWallpaper.type == WallpaperType.kDefault)) {
      store.dispatch(setUpdatedDailyRefreshImageAction());
    }
    // Daily Refresh state should also get updated when wallpaper changes.
    getDailyRefreshState(getWallpaperProvider(), store);
  }

  static shutdown() {
    if (instance) {
      instance.receiver_.$.close();
      instance = null;
    }
  }
}
