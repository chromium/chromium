// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {CurrentBacklightState} from '../../personalization_app.mojom-webui.js';

/**
 * Stores keyboard backlight related states.
 */
export interface KeyboardBacklightState {
  currentBacklightState: CurrentBacklightState|null;
  shouldShowNudge: boolean;
  wallpaperColor: SkColor|null;
}

export function emptyState(): KeyboardBacklightState {
  return {
    currentBacklightState: null,
    shouldShowNudge: false,
    wallpaperColor: null,
  };
}
