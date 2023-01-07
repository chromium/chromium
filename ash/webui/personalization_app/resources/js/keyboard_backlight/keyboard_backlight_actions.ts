// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/ash/common/store/store.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {BacklightColor} from '../personalization_app.mojom-webui.js';


/**
 * @fileoverview Defines the actions to update keyboard backlight settings.
 */

export enum KeyboardBacklightActionName {
  SET_BACKLIGHT_COLOR = 'set_backlight_color',
  SET_SHOULD_SHOW_NUDGE = 'set_should_show_nudge',
  SET_WALLPAPER_COLOR = 'set_wallpaper_color',
}

export type KeyboardBacklightActions =
    SetBacklightColorAction|SetShouldShowNudgeAction|SetWallpaperColorAction;

export type SetBacklightColorAction = Action&{
  name: KeyboardBacklightActionName.SET_BACKLIGHT_COLOR,
  backlightColor: BacklightColor,
};

export type SetShouldShowNudgeAction = Action&{
  name: KeyboardBacklightActionName.SET_SHOULD_SHOW_NUDGE,
  shouldShowNudge: boolean,
};

export type SetWallpaperColorAction = Action&{
  name: KeyboardBacklightActionName.SET_WALLPAPER_COLOR,
  wallpaperColor: SkColor,
};

/**
 * Sets the current value of the backlight color.
 */
export function setBacklightColorAction(backlightColor: BacklightColor):
    SetBacklightColorAction {
  return {
    name: KeyboardBacklightActionName.SET_BACKLIGHT_COLOR,
    backlightColor,
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
