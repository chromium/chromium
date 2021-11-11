// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview TODO(cowmoo)
 */


import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {WallpaperCollection, WallpaperImage} from '../trusted/personalization_app.mojom-webui.js';

export const untrustedOrigin = 'chrome-untrusted://personalization';

export const trustedOrigin = 'chrome://personalization';

export const kMaximumLocalImagePreviews = 3;

export enum EventType {
  SEND_COLLECTIONS = 'send_collections',
  SEND_GOOGLE_PHOTOS_COUNT = 'send_google_photos_count',
  SEND_GOOGLE_PHOTOS_PHOTOS = 'send_google_photos_photos',
  SELECT_COLLECTION = 'select_collection',
  SELECT_GOOGLE_PHOTOS_COLLECTION = 'select_google_photos_collection',
  SELECT_LOCAL_COLLECTION = 'select_local_collection',
  SEND_IMAGE_COUNTS = 'send_image_counts',
  SEND_IMAGES = 'send_images',
  SEND_LOCAL_IMAGE_DATA = 'send_local_image_data',
  SEND_LOCAL_IMAGES = 'send_local_images',
  SEND_CURRENT_WALLPAPER_ASSET_ID = 'send_current_wallpaper_asset_id',
  SEND_PENDING_WALLPAPER_ASSET_ID = 'send_pending_wallpaper_asset_id',
  SELECT_IMAGE = 'select_image',
  SELECT_LOCAL_IMAGE = 'select_local_image',
  SEND_VISIBLE = 'send_visible',
}

type BaseEvent = {
  type: EventType,
};

export type SendCollectionsEvent = BaseEvent&{
  collections: WallpaperCollection[],
};

export type SendGooglePhotosCountEvent = BaseEvent&{
  count?: number,
};

export type SendGooglePhotosPhotosEvent = BaseEvent&{
  photos?: any[],
};

export type SelectCollectionEvent = BaseEvent&{
  collectionId: string,
};

export type SelectGooglePhotosCollectionEvent = BaseEvent;

export type SelectLocalCollectionEvent = BaseEvent;

export type SendImageCountsEvent = BaseEvent&{
  counts: {[key: string]: number},
};

export type SendImagesEvent = BaseEvent&{
  images: WallpaperImage[],
};

export type SendLocalImagesEvent = BaseEvent&{
  images: FilePath[],
};

/**
 * Sends local image data keyed by stringified local image path.
 */
export type SendLocalImageDataEvent = BaseEvent&{
  data: {[key: string]: string},
};

export type SendCurrentWallpaperAssetIdEvent = BaseEvent&{
  assetId?: bigint,
};

export type SendPendingWallpaperAssetIdEvent = BaseEvent&{
  assetId?: bigint,
};

export type SelectImageEvent = BaseEvent&{
  assetId: bigint,
};

/**
 * Notify an iframe if its visible state changes.
 */
export type SendVisibleEvent = BaseEvent&{
  visible: boolean,
};

/**
 * A displayable type constructed from WallpaperImages to display them as a
 * single unit. e.g. Dark/Light wallpaper images.
 */
export type ImageTile = {
  assetId: bigint,
  attribution: string[],
  unitId: bigint,
  preview: Url[],
};
