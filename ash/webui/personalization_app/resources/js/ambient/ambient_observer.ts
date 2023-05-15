// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {AmbientModeAlbum, AmbientObserverInterface, AmbientObserverReceiver, AmbientProviderInterface, AmbientUiVisibility, AnimationTheme, TemperatureUnit, TopicSource} from '../../personalization_app.mojom-webui.js';
import {isAmbientModeAllowed, isPersonalizationJellyEnabled} from '../load_time_booleans.js';
import {logGooglePhotosPreviewsLoadTime} from '../personalization_metrics_logger.js';
import {Paths} from '../personalization_router_element.js';
import {PersonalizationStore} from '../personalization_store.js';
import {isNonEmptyArray, isRecentHighlightsAlbum} from '../utils.js';

import {setAlbumsAction, setAmbientModeEnabledAction, setAmbientUiVisibilityAction, setAnimationThemeAction, setPreviewsAction, setScreenSaverDurationAction, setTemperatureUnitAction, setTopicSourceAction} from './ambient_actions.js';
import {getAmbientProvider} from './ambient_interface_provider.js';

/** @fileoverview listens for updates on ambient mode changes. */

let instance: AmbientObserver|null = null;

/**
 * Observes ambient mode changes and saves updates to PersonalizationStore.
 */
export class AmbientObserver implements AmbientObserverInterface {
  // Allow logging first load performance if the user began on a page where
  // preview images are loaded immediately.
  static shouldLogPreviewsLoadPerformance: boolean =
      window.location.pathname === Paths.ROOT ||
      window.location.pathname === Paths.AMBIENT;

  static initAmbientObserverIfNeeded(): void {
    if (isAmbientModeAllowed() && !instance) {
      instance = new AmbientObserver();
    }
  }

  static shutdown() {
    if (instance) {
      instance.receiver_.$.close();
      instance = null;
    }
  }

  private receiver_: AmbientObserverReceiver;

  constructor() {
    const provider = getAmbientProvider();
    this.receiver_ = this.initReceiver_(provider);
    provider.fetchSettingsAndAlbums();
  }

  private initReceiver_(ambientProvider: AmbientProviderInterface):
      AmbientObserverReceiver {
    const receiver = new AmbientObserverReceiver(this);
    ambientProvider.setAmbientObserver(receiver.$.bindNewPipeAndPassRemote());
    return receiver;
  }

  onAmbientModeEnabledChanged(ambientModeEnabled: boolean) {
    // Only record google photos previews load performance if ambient mode
    // starts enabled.
    AmbientObserver.shouldLogPreviewsLoadPerformance =
        AmbientObserver.shouldLogPreviewsLoadPerformance && ambientModeEnabled;
    const store = PersonalizationStore.getInstance();
    store.dispatch(setAmbientModeEnabledAction(ambientModeEnabled));
  }

  onAnimationThemeChanged(animationTheme: AnimationTheme): void {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setAnimationThemeAction(animationTheme));
  }

  onScreenSaverDurationChanged(minutes: number): void {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setScreenSaverDurationAction(minutes));
  }

  onTopicSourceChanged(topicSource: TopicSource) {
    const store = PersonalizationStore.getInstance();
    // If the first time receiving `topicSource`, allow logging load
    // performance.
    AmbientObserver.shouldLogPreviewsLoadPerformance =
        AmbientObserver.shouldLogPreviewsLoadPerformance &&
        store.data.ambient.topicSource === null &&
        (topicSource === TopicSource.kGooglePhotos ||
         isPersonalizationJellyEnabled());
    store.dispatch(setTopicSourceAction(topicSource));
  }

  onTemperatureUnitChanged(temperatureUnit: TemperatureUnit) {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setTemperatureUnitAction(temperatureUnit));
  }

  onAlbumsChanged(albums: AmbientModeAlbum[]) {
    const store = PersonalizationStore.getInstance();

    // Prevent recent highlights album from constantly changing preview image
    // when albums are refreshed during a single session.
    const oldRecentHighlightsAlbum =
        (store.data.ambient.albums ||
         []).find(album => isRecentHighlightsAlbum(album));
    if (oldRecentHighlightsAlbum) {
      const newRecentHighlightsAlbum =
          albums.find(album => isRecentHighlightsAlbum(album));
      if (newRecentHighlightsAlbum) {
        // Edit by reference.
        newRecentHighlightsAlbum.url = oldRecentHighlightsAlbum.url;
      }
    }

    store.dispatch(setAlbumsAction(albums));
  }

  onPreviewsFetched(previews: Url[]) {
    const store = PersonalizationStore.getInstance();

    // Only log performance metrics if this is the first time receiving google
    // photos previews.
    // When Jelly disabled: log google photos albums only.
    // When Jelly enabled: log both art galleries and google photos albums.
    AmbientObserver.shouldLogPreviewsLoadPerformance =
        AmbientObserver.shouldLogPreviewsLoadPerformance &&
        (!store.data.ambient.previews ||
         store.data.ambient.previews.length === 0);

    store.dispatch(setPreviewsAction(previews));

    if (AmbientObserver.shouldLogPreviewsLoadPerformance &&
        isNonEmptyArray(previews)) {
      logGooglePhotosPreviewsLoadTime();
      AmbientObserver.shouldLogPreviewsLoadPerformance = false;
    }
  }

  onAmbientUiVisibilityChanged(ambientUiVisibility: AmbientUiVisibility) {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setAmbientUiVisibilityAction(ambientUiVisibility));
  }
}
