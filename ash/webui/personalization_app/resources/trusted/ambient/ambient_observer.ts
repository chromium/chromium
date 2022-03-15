// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AmbientModeAlbum, AmbientObserverInterface, AmbientObserverReceiver, AmbientProviderInterface, AnimationTheme, TemperatureUnit, TopicSource} from '../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';

import {setAlbumsAction, setAmbientModeEnabledAction, setAnimationThemeAction, setTemperatureUnitAction, setTopicSourceAction} from './ambient_actions.js';
import {getAmbientProvider} from './ambient_interface_provider.js';

/** @fileoverview listens for updates on color mode changes. */

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

  receiver_: AmbientObserverReceiver = this.initReceiver_(getAmbientProvider());

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
    store.dispatch(setAlbumsAction(albums));
  }
}
