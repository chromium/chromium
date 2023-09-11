// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {ColorScheme} from '../../color_scheme.mojom-webui.js';
import {SampleColorScheme} from '../../personalization_app.mojom-webui.js';

/**
 * Stores theme related states.
 */
export interface ThemeState {
  colorModeAutoScheduleEnabled: boolean|null;
  colorSchemeSelected: ColorScheme|null;
  darkModeEnabled: boolean|null;
  sampleColorSchemes: SampleColorScheme[];
  staticColorSelected: SkColor|null;
}

export function emptyState(): ThemeState {
  return {
    colorModeAutoScheduleEnabled: null,
    colorSchemeSelected: null,
    darkModeEnabled: null,
    sampleColorSchemes: [],
    staticColorSelected: null,
  };
}
