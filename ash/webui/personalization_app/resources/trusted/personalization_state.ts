// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AmbientState, emptyState as emptyAmbientState} from './ambient/ambient_state.js';
import {emptyState as emptyThemeState, ThemeState} from './theme/theme_state.js';
import {emptyState as emptyUserState, UserState} from './user/user_state.js';
import {emptyState as emptyWallpaperState, WallpaperState} from './wallpaper/wallpaper_state.js';

export interface PersonalizationState {
  error: string|null;
  ambient: AmbientState;
  theme: ThemeState;
  user: UserState;
  wallpaper: WallpaperState;
}

export function emptyState(): PersonalizationState {
  return {
    error: null,
    ambient: emptyAmbientState(),
    theme: emptyThemeState(),
    user: emptyUserState(),
    wallpaper: emptyWallpaperState(),
  };
}
