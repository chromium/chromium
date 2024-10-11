// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {ColorScheme} from '../../color_scheme.mojom-webui.js';
import {ThemeProviderInterface} from '../../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';

import {setColorModeAutoScheduleEnabledAction, setColorSchemeAction, setDarkModeEnabledAction, setGeolocationIsUserModifiableAction, setGeolocationPermissionEnabledAction, setSampleColorSchemesAction, setStaticColorAction} from './theme_actions.js';

/**
 * @fileoverview contains all of the functions to interact with C++ side through
 * mojom calls. Handles setting |PersonalizationStore| state in response to
 * mojom data.
 */

export async function initializeData(
    provider: ThemeProviderInterface,
    store: PersonalizationStore): Promise<void> {
  const [
    { enabled },
    { darkModeEnabled },
    { geolocationEnabled },
    { geolocationIsUserModifiable },
  ] = await Promise.all([
    provider.isColorModeAutoScheduleEnabled(),
    provider.isDarkModeEnabled(),
    provider.isGeolocationEnabledForSystemServices(),
    provider.isGeolocationUserModifiable(),
  ]);

  store.beginBatchUpdate();
  store.dispatch(setDarkModeEnabledAction(darkModeEnabled));
  store.dispatch(setColorModeAutoScheduleEnabledAction(enabled));
  store.dispatch(setGeolocationPermissionEnabledAction(geolocationEnabled));
  store.dispatch(
      setGeolocationIsUserModifiableAction(geolocationIsUserModifiable));
  store.endBatchUpdate();
}

export async function initializeDynamicColorData(
    provider: ThemeProviderInterface,
    store: PersonalizationStore): Promise<void> {
  const [{staticColor}, {colorScheme}, {sampleColorSchemes}] =
      await Promise.all([
        provider.getStaticColor(),
        provider.getColorScheme(),
        provider.generateSampleColorSchemes(),
      ]);
  store.beginBatchUpdate();
  store.dispatch(setStaticColorAction(staticColor));
  store.dispatch(setColorSchemeAction(colorScheme));
  store.dispatch(setSampleColorSchemesAction(sampleColorSchemes));
  store.endBatchUpdate();
}

// Disables or enables dark color mode.
export function setColorModePref(
    darkModeEnabled: boolean, provider: ThemeProviderInterface,
    store: PersonalizationStore) {
  provider.setColorModePref(darkModeEnabled);
  // Dispatch action to highlight color mode.
  store.dispatch(setDarkModeEnabledAction(darkModeEnabled));
}

// Disables or enables color mode auto schedule.
export function setColorModeAutoSchedule(
    enabled: boolean, provider: ThemeProviderInterface,
    store: PersonalizationStore) {
  provider.setColorModeAutoScheduleEnabled(enabled);
  // Dispatch action to highlight auto color mode.
  store.dispatch(setColorModeAutoScheduleEnabledAction(enabled));
}

export function setColorSchemePref(
    colorScheme: ColorScheme, provider: ThemeProviderInterface,
    store: PersonalizationStore) {
  provider.setColorScheme(colorScheme);
  store.dispatch(setColorSchemeAction(colorScheme));
}

export function setStaticColorPref(
    staticColor: SkColor, provider: ThemeProviderInterface,
    store: PersonalizationStore) {
  provider.setStaticColor(staticColor);
  store.dispatch(setStaticColorAction(staticColor));
  store.dispatch(setColorSchemeAction(ColorScheme.kStatic));
}

export function enableGeolocationForSystemServices(
    provider: ThemeProviderInterface, store: PersonalizationStore) {
  provider.enableGeolocationForSystemServices();
  store.dispatch(setGeolocationPermissionEnabledAction(true));
}
