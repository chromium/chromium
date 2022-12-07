// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {AmbientModeAlbum, AmbientObserverInterface, AmbientObserverReceiver, AmbientProviderInterface, AmbientUiVisibility, AnimationTheme, TemperatureUnit, TopicSource} from '../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';

import {setAlbumsAction, setAmbientModeEnabledAction, setAmbientUiVisibilityAction, setAnimationThemeAction, setGooglePhotosAlbumsPreviewsAction, setTemperatureUnitAction, setTopicSourceAction} from './ambient_actions.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {isRecentHighlightsAlbum} from './utils.js';

/** @fileoverview listens for updates on ambient mode changes. */

let instance: AmbientObserver|null = null;

/**
 * Observes ambient mode changes and saves updates to PersonalizationStore.
 */
export class AmbientObserver implements AmbientObserverInterface {
  static initAmbientObserverIfNeeded(): void {
    if (!instance) {
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
    const store = PersonalizationStore.getInstance();
    store.dispatch(setAmbientModeEnabledAction(ambientModeEnabled));
  }

  onAnimationThemeChanged(animationTheme: AnimationTheme): void {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setAnimationThemeAction(animationTheme));
  }

  onTopicSourceChanged(topicSource: TopicSource) {
    const store = PersonalizationStore.getInstance();
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

  onGooglePhotosAlbumsPreviewsFetched(previews: Url[]) {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setGooglePhotosAlbumsPreviewsAction(previews));
  }

  onAmbientUiVisibilityChanged(ambientUiVisibility: AmbientUiVisibility) {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setAmbientUiVisibilityAction(ambientUiVisibility));
  }
}
