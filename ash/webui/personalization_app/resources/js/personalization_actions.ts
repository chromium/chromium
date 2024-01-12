// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SeaPenActions} from 'chrome://resources/ash/common/sea_pen/sea_pen_actions.js';
import {Action} from 'chrome://resources/js/store.js';

import {AmbientActions} from './ambient/ambient_actions.js';
import {KeyboardBacklightActions} from './keyboard_backlight/keyboard_backlight_actions.js';
import {PersonalizationStateError} from './personalization_state.js';
import {ThemeActions} from './theme/theme_actions.js';
import {UserActions} from './user/user_actions.js';
import {WallpaperActions} from './wallpaper/wallpaper_actions.js';

/**
 * @fileoverview Defines the actions to change state.
 */
export enum PersonalizationActionName {
  DISMISS_ERROR = 'dismiss_error',
  SET_ERROR = 'set_error',
}

export interface DismissErrorAction extends Action {
  id: string|null;
  fromUser: boolean;
  name: PersonalizationActionName.DISMISS_ERROR;
}

/**
 * Dismiss the current error if there is any.
 * @param id if non-null, the current error is only dismissed if it matches.
 * @param fromUser whether the dismiss action originated from the user.
 */
export function dismissErrorAction(
    id: string|null, fromUser: boolean): DismissErrorAction {
  return {id, fromUser, name: PersonalizationActionName.DISMISS_ERROR};
}

export interface SetErrorAction extends Action {
  error: PersonalizationStateError;
  name: PersonalizationActionName.SET_ERROR;
}

/** Sets the current error. */
export function setErrorAction(error: PersonalizationStateError):
    SetErrorAction {
  return {error, name: PersonalizationActionName.SET_ERROR};
}

export type Actions =
    AmbientActions|KeyboardBacklightActions|ThemeActions|UserActions|
    WallpaperActions|DismissErrorAction|SetErrorAction|SeaPenActions;
