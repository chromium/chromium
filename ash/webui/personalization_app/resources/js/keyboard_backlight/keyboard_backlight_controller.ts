// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BacklightColor, KeyboardBacklightProviderInterface} from '../../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';

import {setCurrentBacklightStateAction, setShouldShowNudgeAction} from './keyboard_backlight_actions.js';

/**
 * @fileoverview contains all of the functions to interact with keyboard
 * backlight related C++ side through mojom calls. Handles setting
 * |PersonalizationStore| state in response to mojom data.
 */

// Set the keyboard backlight color.
export function setBacklightColor(
    backlightColor: BacklightColor,
    provider: KeyboardBacklightProviderInterface, store: PersonalizationStore) {
  provider.setBacklightColor(backlightColor);

  // Dispatch action to set the current backlight state - color.
  store.dispatch(setCurrentBacklightStateAction({color: backlightColor}));
}

// Set the keyboard backlight color for the given zone.
export function setBacklightZoneColor(
    zone: number, backlightColor: BacklightColor, zoneColors: BacklightColor[],
    provider: KeyboardBacklightProviderInterface, store: PersonalizationStore) {
  provider.setBacklightZoneColor(zone, backlightColor);

  // Dispatch action to set the current backlight state - zone colors.
  const updatedZoneColors = [...zoneColors];
  updatedZoneColors[zone] = backlightColor;
  store.dispatch(
      setCurrentBacklightStateAction({zoneColors: updatedZoneColors}));
}

// Set the should show nudge boolean.
export async function getShouldShowNudge(
    provider: KeyboardBacklightProviderInterface, store: PersonalizationStore) {
  const {shouldShowNudge} = await provider.shouldShowNudge();

  // Dispatch action to set the should show nudge boolean.
  store.dispatch(setShouldShowNudgeAction(shouldShowNudge));
}

// Called when the nudge is shown.
export function handleNudgeShown(
    provider: KeyboardBacklightProviderInterface, store: PersonalizationStore) {
  provider.handleNudgeShown();

  // Dispatch action to set the should show nudge boolean.
  store.dispatch(setShouldShowNudgeAction(false));
}
