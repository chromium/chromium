// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import type {ColorScheme} from '../../color_scheme.mojom-webui.js';
import type {SampleColorScheme} from '../../personalization_app.mojom-webui.js';


/**
 * Stores theme related states.
 */
export interface ThemeState {
  colorModeAutoScheduleEnabled: boolean|null;
  colorSchemeSelected: ColorScheme|null;
  darkModeEnabled: boolean|null;
  sampleColorSchemes: SampleColorScheme[];
  staticColorSelected: SkColor|null;
  geolocationPermissionEnabled: boolean|null;
  sunriseTime: string|null;
  sunsetTime: string|null;
  geolocationIsUserModifiable: boolean|null;
}

export function emptyState(): ThemeState {
  return {
    colorModeAutoScheduleEnabled: null,
    colorSchemeSelected: null,
    darkModeEnabled: null,
    sampleColorSchemes: [],
    staticColorSelected: null,
    geolocationPermissionEnabled: null,
    sunriseTime: null,
    sunsetTime: null,
    geolocationIsUserModifiable: null,
  };
}
