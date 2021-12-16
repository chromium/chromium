// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {emptyState as emptyWallpaperState, WallpaperState} from './wallpaper/wallpaper_state.js';

export interface PersonalizationState {
  wallpaper: WallpaperState;
  error: string|null;
}

export function emptyState(): PersonalizationState {
  return {
    wallpaper: emptyWallpaperState(),
    error: null,
  };
}
