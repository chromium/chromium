// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/cr/ui/store.js';

import {AmbientActions} from './ambient/ambient_actions.js';
import {ThemeActions} from './theme/theme_actions.js';
import {UserActions} from './user/user_actions.js';
import {WallpaperActions} from './wallpaper/wallpaper_actions.js';

/**
 * @fileoverview Defines the actions to change state.
 */
export enum PersonalizationActionName {
  DISMISS_ERROR = 'dismiss_error',
}

export type DismissErrorAction = Action&{
  name: PersonalizationActionName.DISMISS_ERROR,
};

/**
 * Dismiss the current error if there is any.
 */
export function dismissErrorAction(): DismissErrorAction {
  return {name: PersonalizationActionName.DISMISS_ERROR};
}

export type Actions =
    AmbientActions|ThemeActions|UserActions|WallpaperActions|DismissErrorAction;
