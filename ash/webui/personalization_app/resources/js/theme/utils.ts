// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions to be used for theme related components.
 */
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';

import {ColorScheme} from '../../color_scheme.mojom-webui.js';

export function isAutomaticSeedColorEnabled(colorScheme: ColorScheme|null) {
  return colorScheme === null || colorScheme !== ColorScheme.kStatic;
}

export const DEFAULT_STATIC_COLOR = hexColorToSkColor('#4285f4');
export const DEFAULT_COLOR_SCHEME = ColorScheme.kTonalSpot;
