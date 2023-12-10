// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/store.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {CurrentBacklightState} from '../../personalization_app.mojom-webui.js';

/**
 * @fileoverview Defines the actions to update keyboard backlight settings.
 */

export enum KeyboardBacklightActionName {
  SET_CURRENT_BACKLIGHT_STATE = 'set_current_backlight_state',
  SET_SHOULD_SHOW_NUDGE = 'set_should_show_nudge',
  SET_WALLPAPER_COLOR = 'set_wallpaper_color',
}

export type KeyboardBacklightActions = SetCurrentBacklightStateAction|
    SetShouldShowNudgeAction|SetWallpaperColorAction;

export interface SetCurrentBacklightStateAction extends Action {
  name: KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE;
  currentBacklightState: CurrentBacklightState;
}


export interface SetShouldShowNudgeAction extends Action {
  name: KeyboardBacklightActionName.SET_SHOULD_SHOW_NUDGE;
  shouldShowNudge: boolean;
}


export interface SetWallpaperColorAction extends Action {
  name: KeyboardBacklightActionName.SET_WALLPAPER_COLOR;
  wallpaperColor: SkColor;
}


/**
 * Sets the current value of the backlight state.
 */
export function setCurrentBacklightStateAction(
    currentBacklightState: CurrentBacklightState):
    SetCurrentBacklightStateAction {
  return {
    name: KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE,
    currentBacklightState,
  };
}

export function setShouldShowNudgeAction(shouldShowNudge: boolean):
    SetShouldShowNudgeAction {
  return {
    name: KeyboardBacklightActionName.SET_SHOULD_SHOW_NUDGE,
    shouldShowNudge,
  };
}

/**
 * Sets the current value of the wallpaper extracted color.
 */
export function setWallpaperColorAction(wallpaperColor: SkColor):
    SetWallpaperColorAction {
  return {
    name: KeyboardBacklightActionName.SET_WALLPAPER_COLOR,
    wallpaperColor,
  };
}
