// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Wallpaper related utility functions in personalization app */

import {assert} from 'chrome://resources/js/assert_ts.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {CurrentWallpaper, GooglePhotosAlbum, GooglePhotosPhoto, WallpaperImage, WallpaperLayout, WallpaperType} from '../personalization_app.mojom-webui.js';
import {getNumberOfGridItemsPerRow, isNonEmptyArray, isNonEmptyString} from '../utils.js';

import {DefaultImageSymbol, DisplayableImage, kDefaultImageSymbol} from './constants.js';

export function isWallpaperImage(obj: any): obj is WallpaperImage {
  return !!obj && typeof obj.assetId === 'bigint';
}

export function isFilePath(obj: any): obj is FilePath {
  return !!obj && typeof obj.path === 'string' && obj.path;
}

export function isDefaultImage(obj: any): obj is DefaultImageSymbol {
  return obj === kDefaultImageSymbol;
}

/** Checks whether |obj| is an instance of |GooglePhotosPhoto|. */
export function isGooglePhotosPhoto(obj: any): obj is GooglePhotosPhoto {
  return !!obj && typeof obj.id === 'string';
}

/** Returns whether |image| is a match for the specified |key|. */
export function isImageAMatchForKey(
    image: DisplayableImage, key: string|DefaultImageSymbol): boolean {
  if (isWallpaperImage(image)) {
    return key === image.assetId.toString();
  }
  if (isDefaultImage(image)) {
    return key === kDefaultImageSymbol;
  }
  if (isFilePath(image)) {
    return key === image.path;
  }
  assert(isGooglePhotosPhoto(image));
  // NOTE: Old clients may not support |dedupKey| when setting Google Photos
  // wallpaper, so use |id| in such cases for backwards compatibility.
  return (image.dedupKey && key === image.dedupKey) || key === image.id;
}

/**
 * Compare an image from the list of selectable images with the currently
 * selected user wallpaper.
 * @param image a selectable image that the user can choose
 * @param selected currently selected user walpaper
 * @return boolean whether they are considered the same image
 */
export function isImageEqualToSelected(
    image: DisplayableImage, selected: CurrentWallpaper): boolean {
  if (isDefaultImage(image)) {
    // Special case for default images. Mojom generated code for type
    // |CurrentWallpaper.key| cannot include javascript symbols.
    return selected.type === WallpaperType.kDefault;
  }
  return isImageAMatchForKey(image, selected.key);
}

/**
 * Subtly different than |getImageKey|, which returns just the file part of the
 * path. |getPathOrSymbol| returns the whole path for local images.
 */
export function getPathOrSymbol(image: FilePath|DefaultImageSymbol): string|
    DefaultImageSymbol {
  if (isFilePath(image)) {
    return image.path;
  }
  assert(image === kDefaultImageSymbol, 'only one symbol should be present');
  return image;
}

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

/** Returns a css variable to control the animation delay. */
export function getLoadingPlaceholderAnimationDelay(index: number): string {
  // 48 is chosen because 4 and 3 are both factors, and it's large enough
  // that 48 grid items don't fit on one screen.
  const wrapped = index % 48;
  return `--animation-delay: ${wrapped * 83}ms;`;
}

/**
 * Returns loading placeholders to render given the current inner width of the
 * |window|. Placeholders are constructed using the specified |factory|.
 */
export function getLoadingPlaceholders<T>(factory: () => T): T[] {
  const x = getNumberOfGridItemsPerRow();
  const y = Math.floor(window.innerHeight / /*tileHeightPx=*/ 136);
  return Array.from({length: x * y}, factory);
}

/**
 * Returns the attribution list from local storage.
 * Such as attribution (image title, author...) of a downloaded image.
 */
export function getLocalStorageAttribution(key: string): string[] {
  const attributionMap =
      JSON.parse((window.localStorage['attribution'] || '{}'));
  const attribution = attributionMap[key];
  if (!attribution) {
    console.warn('Unable to get attribution from local storage.', key);
  }
  return attribution;
}

/**
 * Get a url to download a high quality preview of the current wallpaper.
 * Responds with null in case |image| is invalid or null.
 */
export function getWallpaperSrc(image: CurrentWallpaper|null): string|null {
  if (!image) {
    return null;
  }
  if (typeof image.key !== 'string' || !image.key) {
    console.warn('Invalid image key received');
    return null;
  }
  // Add a key query parameter to cache bust when the image changes.
  return `/wallpaper.jpg?key=${encodeURIComponent(image.key)}`;
}

/**
 * Finds and returns a Google Photos album from albums list with a matching id.
 * Returns null in case invalid id or albums list or no album is found.
 */
export function findAlbumById(
    albumId: string|undefined,
    albums: GooglePhotosAlbum[]|null|undefined): GooglePhotosAlbum|null {
  if (isNonEmptyString(albumId) && isNonEmptyArray(albums)) {
    return albums.find(album => album.id === albumId) ?? null;
  }
  return null;
}
