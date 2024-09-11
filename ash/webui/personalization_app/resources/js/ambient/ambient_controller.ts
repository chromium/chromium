// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {AmbientModeAlbum, AmbientProviderInterface, AmbientTheme, TemperatureUnit, TopicSource} from '../../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';

import {setAlbumSelectedAction, setAmbientModeEnabledAction, setAmbientThemeAction, setGeolocationIsUserModifiableAction, setGeolocationPermissionEnabledAction, setScreenSaverDurationAction, setShouldShowTimeOfDayBannerAction, setTemperatureUnitAction, setTopicSourceAction} from './ambient_actions.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {isValidTopicSourceAndTheme} from './utils.js';



/**
 * @fileoverview contains all of the functions to interact with ambient mode
 * related C++ side through mojom calls. Handles setting |PersonalizationStore|
 * state in response to mojom data.
 */

export async function initializeData(
    provider: AmbientProviderInterface,
    store: PersonalizationStore): Promise<void> {
  const [{geolocationEnabled}, {geolocationIsUserModifiable}] =
      await Promise.all([
        provider.isGeolocationEnabledForSystemServices(),
        provider.isGeolocationUserModifiable(),
      ]);
  store.beginBatchUpdate();
  store.dispatch(setGeolocationPermissionEnabledAction(geolocationEnabled));
  store.dispatch(
      setGeolocationIsUserModifiableAction(geolocationIsUserModifiable));
  store.endBatchUpdate();
}

// Enable or disable ambient mode.
export async function setAmbientModeEnabled(
    ambientModeEnabled: boolean, provider: AmbientProviderInterface,
    store: PersonalizationStore): Promise<void> {
  provider.setAmbientModeEnabled(ambientModeEnabled);

  // Dispatch action to toggle the button to indicate if the ambient mode is
  // enabled.
  store.dispatch(setAmbientModeEnabledAction(ambientModeEnabled));
}

// Set the ambient theme.
export function setAmbientTheme(
    ambientTheme: AmbientTheme, provider: AmbientProviderInterface,
    store: PersonalizationStore): void {
  provider.setAmbientTheme(ambientTheme);

  store.dispatch(setAmbientThemeAction(ambientTheme));
}

// Set ambient mode screen saver running duration in minutes.
export function setScreenSaverDuration(
    minutes: number, provider: AmbientProviderInterface,
    store: PersonalizationStore): void {
  provider.setScreenSaverDuration(minutes);
  store.dispatch(setScreenSaverDurationAction(minutes));
}

// Set ambient mode topic source.
export function setTopicSource(
    topicSource: TopicSource, provider: AmbientProviderInterface,
    store: PersonalizationStore): void {
  assert(
      isValidTopicSourceAndTheme(topicSource, store.data.ambient.ambientTheme),
      'invalid topic source and ambient theme combination');
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

export async function getShouldShowTimeOfDayBanner(
    store: PersonalizationStore) {
  const {shouldShowBanner} =
      await getAmbientProvider().shouldShowTimeOfDayBanner();

  // Dispatch action to set the should show banner boolean.
  store.dispatch(setShouldShowTimeOfDayBannerAction(shouldShowBanner));
}


// Sets shouldShowTimeOfDayBanner to false.
export function dismissTimeOfDayBanner(store: PersonalizationStore): void {
  if (!store.data.ambient.shouldShowTimeOfDayBanner) {
    // Do nothing if the banner is already dismissed;
    return;
  }
  getAmbientProvider().handleTimeOfDayBannerDismissed();

  store.dispatch(setShouldShowTimeOfDayBannerAction(false));
}

export function enableGeolocationForSystemServices(
    store: PersonalizationStore) {
  getAmbientProvider().enableGeolocationForSystemServices();
  store.dispatch(setGeolocationPermissionEnabledAction(true));
}
