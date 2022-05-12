// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {GooglePhotosEnablementState, GooglePhotosPhoto, WallpaperCollection, WallpaperImage} from '../trusted/personalization_app.mojom-webui.js';

// A special unique symbol that represents the device default image, normally
// not accessible by the user.
// Warning: symbols as object keys are not iterated by normal methods, but can
// be iterated by |getOwnPropertySymbols|.
export const kDefaultImageSymbol: unique symbol =
    Symbol.for('chromeos_default_wallpaper');

export type DefaultImageSymbol = typeof kDefaultImageSymbol;

export type DisplayableImage =
    FilePath|GooglePhotosPhoto|WallpaperImage|DefaultImageSymbol;

export const trustedOrigin = 'chrome://personalization';

export const kMaximumLocalImagePreviews = 4;

export enum EventType {
  SEND_COLLECTIONS = 'send_collections',
  SEND_GOOGLE_PHOTOS_ENABLED = 'send_google_photos_enabled',
  SELECT_COLLECTION = 'select_collection',
  SELECT_GOOGLE_PHOTOS_COLLECTION = 'select_google_photos_collection',
  SELECT_LOCAL_COLLECTION = 'select_local_collection',
  SEND_IMAGE_COUNTS = 'send_image_counts',
  SEND_IMAGE_TILES = 'send_image_tiles',
  SEND_LOCAL_IMAGE_DATA = 'send_local_image_data',
  SEND_LOCAL_IMAGES = 'send_local_images',
  SEND_CURRENT_WALLPAPER_ASSET_ID = 'send_current_wallpaper_asset_id',
  SEND_PENDING_WALLPAPER_ASSET_ID = 'send_pending_wallpaper_asset_id',
  SELECT_IMAGE = 'select_image',
  SELECT_LOCAL_IMAGE = 'select_local_image',
  SEND_VISIBLE = 'send_visible',
}

export type SendCollectionsEvent = {
  type: EventType.SEND_COLLECTIONS,
  collections: WallpaperCollection[],
};

export type SendGooglePhotosEnabledEvent = {
  type: EventType.SEND_GOOGLE_PHOTOS_ENABLED,
  enabled: GooglePhotosEnablementState,
};

export type SelectCollectionEvent = {
  type: EventType.SELECT_COLLECTION,
  collectionId: string,
};

export type SelectGooglePhotosCollectionEvent = {
  type: EventType.SELECT_GOOGLE_PHOTOS_COLLECTION,
};

export type SelectLocalCollectionEvent = {
  type: EventType.SELECT_LOCAL_COLLECTION,
};

export type SendImageCountsEvent = {
  type: EventType.SEND_IMAGE_COUNTS,
  counts: {[key: string]: number|null},
};

/**
 * A displayable type constructed from WallpaperImages to display them as a
 * single unit. e.g. Dark/Light wallpaper images.
 */
export type ImageTile = {
  assetId?: bigint,
  attribution?: string[],
  unitId?: bigint, preview: Url[],
};

export type SendImageTilesEvent = {
  type: EventType.SEND_IMAGE_TILES,
  tiles: ImageTile[],
};

export type SendLocalImagesEvent = {
  type: EventType.SEND_LOCAL_IMAGES,
  images: Array<FilePath|DefaultImageSymbol>,
};

/**
 * Sends local image data keyed by stringified local image path.
 */
export type SendLocalImageDataEvent = {
  type: EventType.SEND_LOCAL_IMAGE_DATA,
  data: Record<string|DefaultImageSymbol, string>,
};

export type SendCurrentWallpaperAssetIdEvent = {
  type: EventType.SEND_CURRENT_WALLPAPER_ASSET_ID,
  assetId?: bigint,
};

export type SendPendingWallpaperAssetIdEvent = {
  type: EventType.SEND_PENDING_WALLPAPER_ASSET_ID,
  assetId?: bigint,
};

export type SelectImageEvent = {
  type: EventType.SELECT_IMAGE,
  assetId: bigint,
};

/**
 * Notify an iframe if its visible state changes.
 */
export type SendVisibleEvent = {
  type: EventType.SEND_VISIBLE,
  visible: boolean,
};

export type Events =
    SendCollectionsEvent|SendGooglePhotosEnabledEvent|SelectCollectionEvent|
    SelectGooglePhotosCollectionEvent|SelectLocalCollectionEvent|
    SendImageCountsEvent|SendImageTilesEvent|SendLocalImagesEvent|
    SendLocalImageDataEvent|SendCurrentWallpaperAssetIdEvent|
    SendPendingWallpaperAssetIdEvent|SelectImageEvent|SendVisibleEvent;
