// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions to be used in trusted code.
 */

import {WallpaperLayout} from '../trusted/personalization_app.mojom-webui.js';
/**
 * @param {string} layout
 * @return {WallpaperLayout}
 */
export function getWallpaperLayoutEnum(layout) {
  switch (layout) {
    case 'FILL':
      return WallpaperLayout.kCenterCropped;
    case 'CENTER':  // fall through
    default:
      return WallpaperLayout.kCenter;
  }
}
