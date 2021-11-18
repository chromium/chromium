// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions to be used in trusted code.
 */

import {WallpaperLayout} from '../trusted/personalization_app.mojom-webui.js';
/**
 * Convert a string layout value to the corresponding enum.
 */
export function getWallpaperLayoutEnum(layout: string): WallpaperLayout {
  switch (layout) {
    case 'FILL':
      return WallpaperLayout.kCenterCropped;
    case 'CENTER':  // fall through
    default:
      return WallpaperLayout.kCenter;
  }
}

/**
 * Checks if argument is a string with non-zero length.
 */
export function isNonEmptyString(maybeString: unknown): maybeString is string {
  return typeof maybeString === 'string' && maybeString.length > 0;
}
