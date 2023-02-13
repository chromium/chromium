// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AmbientModeAlbum, AmbientProviderInterface, AnimationTheme, TemperatureUnit, TopicSource} from '../../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';

import {setAlbumSelectedAction, setAmbientModeEnabledAction, setAnimationThemeAction, setTemperatureUnitAction, setTopicSourceAction} from './ambient_actions.js';

/**
 * @fileoverview contains all of the functions to interact with ambient mode
 * related C++ side through mojom calls. Handles setting |PersonalizationStore|
 * state in response to mojom data.
 */

// Enable or disable ambient mode.
export async function setAmbientModeEnabled(
    ambientModeEnabled: boolean, provider: AmbientProviderInterface,
    store: PersonalizationStore): Promise<void> {
  provider.setAmbientModeEnabled(ambientModeEnabled);

  // Dispatch action to toggle the button to indicate if the ambient mode is
  // enabled.
  store.dispatch(setAmbientModeEnabledAction(ambientModeEnabled));
}

// Set the animation theme.
export function setAnimationTheme(
    animationTheme: AnimationTheme, provider: AmbientProviderInterface,
    store: PersonalizationStore): void {
  provider.setAnimationTheme(animationTheme);

  store.dispatch(setAnimationThemeAction(animationTheme));
}

// Set ambient mode topic source.
export function setTopicSource(
    topicSource: TopicSource, provider: AmbientProviderInterface,
    store: PersonalizationStore): void {
  provider.setTopicSource(topicSource);

  // Dispatch action to select topic source.
  store.dispatch(setTopicSourceAction(topicSource));
}

// Set ambient mode temperature unit.
export function setTemperatureUnit(
    temperatureUnit: TemperatureUnit, provider: AmbientProviderInterface,
    store: PersonalizationStore): void {
  // Dispatch action to select temperature unit.
  store.dispatch(setTemperatureUnitAction(temperatureUnit));

  provider.setTemperatureUnit(temperatureUnit);
}

// Set one album as selected or not.
export function setAlbumSelected(
    album: AmbientModeAlbum, provider: AmbientProviderInterface,
    store: PersonalizationStore): void {
  // Dispatch action to update albums info with the changed album.
  store.dispatch(setAlbumSelectedAction());

  provider.setAlbumSelected(album.id, album.topicSource, album.checked);
}

// Start screen saver preview.
export function startScreenSaverPreview(provider: AmbientProviderInterface):
    void {
  provider.startScreenSaverPreview();
}
