// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Wallpaper related utility functions in personalization app */

import {isNonEmptyArray, isNonEmptyFilePath} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {CurrentAttribution, CurrentWallpaper, GooglePhotosAlbum, GooglePhotosPhoto, WallpaperImage, WallpaperLayout, WallpaperType} from '../../personalization_app.mojom-webui.js';
import {getNumberOfGridItemsPerRow, isNonEmptyString} from '../utils.js';

import {DefaultImageSymbol, DisplayableImage, kDefaultImageSymbol} from './constants.js';
import {DailyRefreshState} from './wallpaper_state.js';

export function isWallpaperImage(obj: any): obj is WallpaperImage {
  return !!obj && typeof obj.unitId === 'bigint';
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
    return key === image.unitId.toString();
  }
  if (isDefaultImage(image)) {
    return key === kDefaultImageSymbol;
  }
  if (isNonEmptyFilePath(image)) {
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
  if (isNonEmptyFilePath(image)) {
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
  const y = Math.max(Math.floor(window.innerHeight / /*tileHeightPx=*/ 136), 2);
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
 * Get the aria label of the currently selected wallpaper.
 */
export function getWallpaperAriaLabel(
    image: CurrentWallpaper|null, attribution: CurrentAttribution|null,
    dailyRefreshState: DailyRefreshState|null): string {
  if (!image || !attribution || image.key !== attribution.key) {
    return `${loadTimeData.getString('currentlySet')} ${
        loadTimeData.getString('unknownImageAttribution')}`;
  }
  if (image.type === WallpaperType.kDefault) {
    return `${loadTimeData.getString('currentlySet')} ${
        loadTimeData.getString('defaultWallpaper')}`;
  }
  const isDailyRefreshActive = !!dailyRefreshState;
  if (isNonEmptyArray(attribution.attribution)) {
    return isDailyRefreshActive ?
        [
          loadTimeData.getString('currentlySet'),
          loadTimeData.getString('dailyRefresh'),
          ...attribution.attribution,
        ].join(' ') :
        [
          loadTimeData.getString('currentlySet'),
          ...attribution.attribution,
        ].join(' ');
  }
  // Fallback to cached attribution.
  const cachedAttribution = getLocalStorageAttribution(image.key);
  if (isNonEmptyArray(cachedAttribution)) {
    return isDailyRefreshActive ?
        [
          loadTimeData.getString('currentlySet'),
          loadTimeData.getString('dailyRefresh'),
          ...attribution.attribution,
        ].join(' ') :
        [loadTimeData.getString('currentlySet'), ...cachedAttribution].join(
            ' ');
  }
  return `${loadTimeData.getString('currentlySet')} ${
      loadTimeData.getString('unknownImageAttribution')}`;
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
  /**
   * Add a key query parameter to cache bust when the image changes.
   *
   * TODO(b/276360067): UnitId is used as the key for online wallpaper images so
   * we need to bust the cache to show the correct variant when it changes.
   * Remove &timestamp param after b/276360067 is implemented.
   */
  return `/wallpaper.jpg?key=${encodeURIComponent(image.key)}&${Date.now()}`;
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
