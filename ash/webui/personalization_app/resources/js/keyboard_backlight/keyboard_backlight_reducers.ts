// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {CurrentBacklightState} from '../../personalization_app.mojom-webui.js';
import {Actions} from '../personalization_actions.js';
import {ReducerFunction} from '../personalization_reducers.js';
import {PersonalizationState} from '../personalization_state.js';

import {KeyboardBacklightActionName} from './keyboard_backlight_actions.js';
import {KeyboardBacklightState} from './keyboard_backlight_state.js';

export function currentBacklightStateReducer(
    state: CurrentBacklightState|null, action: Actions,
    _: PersonalizationState): CurrentBacklightState|null {
  switch (action.name) {
    case KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE:
      return action.currentBacklightState;
    default:
      return state;
  }
}

export function shouldShowNudgeReducer(
    state: boolean, action: Actions, _: PersonalizationState): boolean {
  switch (action.name) {
    case KeyboardBacklightActionName.SET_SHOULD_SHOW_NUDGE:
      return action.shouldShowNudge;
    default:
      return state;
  }
}

export function wallpaperColorReducer(
    state: SkColor|null, action: Actions, _: PersonalizationState): SkColor|
    null {
  switch (action.name) {
    case KeyboardBacklightActionName.SET_WALLPAPER_COLOR:
      return action.wallpaperColor;
    default:
      return state;
  }
}

export const keyboardBacklightReducers: {
  [K in keyof KeyboardBacklightState]:
      ReducerFunction<KeyboardBacklightState[K]>
} = {
  currentBacklightState: currentBacklightStateReducer,
  shouldShowNudge: shouldShowNudgeReducer,
  wallpaperColor: wallpaperColorReducer,
};
