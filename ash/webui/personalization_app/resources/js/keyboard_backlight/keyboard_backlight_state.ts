// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {BacklightColor} from '../personalization_app.mojom-webui.js';

/**
 * Stores keyboard backlight related states.
 */
export interface KeyboardBacklightState {
  backlightColor: BacklightColor|null;
  shouldShowNudge: boolean;
  wallpaperColor: SkColor|null;
}

export function emptyState(): KeyboardBacklightState {
  return {
    backlightColor: null,
    shouldShowNudge: false,
    wallpaperColor: null,
  };
}
