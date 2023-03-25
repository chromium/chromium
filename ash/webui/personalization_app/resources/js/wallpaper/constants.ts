// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {GooglePhotosPhoto, WallpaperImage} from '../../personalization_app.mojom-webui.js';

// A special unique symbol that represents the device default image, normally
// not accessible by the user.
// Warning: symbols as object keys are not iterated by normal methods, but can
// be iterated by |getOwnPropertySymbols|.
export const kDefaultImageSymbol: unique symbol =
    Symbol.for('chromeos_default_wallpaper');

export type DefaultImageSymbol = typeof kDefaultImageSymbol;

export type DisplayableImage =
    FilePath|GooglePhotosPhoto|WallpaperImage|DefaultImageSymbol;

export const kMaximumLocalImagePreviews = 4;

/**
 * A displayable type constructed from WallpaperImages to display them as a
 * single unit. e.g. Dark/Light wallpaper images.
 */
export interface ImageTile {
  assetId?: bigint;
  attribution?: string[];
  unitId?: bigint;
  preview: Url[];
  isTimeOfDayWallpaper?: boolean;
  hasPreviewImage?: boolean;
}
