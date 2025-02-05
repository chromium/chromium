// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AmbientState} from './ambient/ambient_state.js';
import {emptyState as emptyAmbientState} from './ambient/ambient_state.js';
import type {KeyboardBacklightState} from './keyboard_backlight/keyboard_backlight_state.js';
import {emptyState as emptyKeyboardBacklightState} from './keyboard_backlight/keyboard_backlight_state.js';
import type {ThemeState} from './theme/theme_state.js';
import {emptyState as emptyThemeState} from './theme/theme_state.js';
import type {UserState} from './user/user_state.js';
import {emptyState as emptyUserState} from './user/user_state.js';
import type {WallpaperState} from './wallpaper/wallpaper_state.js';
import {emptyState as emptyWallpaperState} from './wallpaper/wallpaper_state.js';

/**
 * Interface for an error.
 * |id| - identifier for error.
 * |message| - user facing message for error.
 * |dismiss.message| - user facing message for dismiss button.
 * |dismiss.callback| - callback invoked on error dismissal.
 */
export interface PersonalizationStateError {
  id?: string;
  message: string;
  dismiss?: {
    message?: string,
    callback?: (fromUser: boolean) => void,
  };
}

export interface PersonalizationState {
  error: PersonalizationStateError|null;
  ambient: AmbientState;
  keyboardBacklight: KeyboardBacklightState;
  theme: ThemeState;
  user: UserState;
  wallpaper: WallpaperState;
}

export function emptyState(): PersonalizationState {
  return {
    error: null,
    ambient: emptyAmbientState(),
    keyboardBacklight: emptyKeyboardBacklightState(),
    theme: emptyThemeState(),
    user: emptyUserState(),
    wallpaper: emptyWallpaperState(),
  };
}
