// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/ash/common/store/store.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {CurrentWallpaper, GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, WallpaperCollection, WallpaperImage} from '../personalization_app.mojom-webui.js';

import {DisplayableImage} from './constants.js';

/**
 * @fileoverview Defines the actions to change wallpaper state.
 */

export enum WallpaperActionName {
  APPEND_GOOGLE_PHOTOS_ALBUM = 'append_google_photos_album',
  APPEND_GOOGLE_PHOTOS_ALBUMS = 'append_google_photos_albums',
  APPEND_GOOGLE_PHOTOS_PHOTOS = 'append_google_photos_photos',
  BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM = 'begin_load_google_photos_album',
  BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS = 'begin_load_google_photos_albums',
  BEGIN_LOAD_GOOGLE_PHOTOS_ENABLED = 'begin_load_google_photos_enabled',
  BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS = 'begin_load_google_photos_photos',
  BEGIN_LOAD_IMAGES_FOR_COLLECTIONS = 'begin_load_images_for_collections',
  BEGIN_LOAD_DEFAULT_IMAGE_THUMBNAIL = 'begin_load_default_image',
  BEGIN_LOAD_LOCAL_IMAGES = 'begin_load_local_images',
  BEGIN_LOAD_LOCAL_IMAGE_DATA = 'begin_load_local_image_data',
  BEGIN_LOAD_SELECTED_IMAGE = 'begin_load_selected_image',
  BEGIN_SELECT_IMAGE = 'begin_select_image',
  BEGIN_UPDATE_DAILY_REFRESH_IMAGE = 'begin_update_daily_refresh_image',
  CLEAR_DAILY_REFRESH_ACTION = 'clear_daily_refresh_action',
  END_SELECT_IMAGE = 'end_select_image',
  SET_COLLECTIONS = 'set_collections',
  SET_DAILY_REFRESH_COLLECTION_ID = 'set_daily_refresh_collection_id',
  SET_GOOGLE_PHOTOS_DAILY_REFRESH_ALBUM_ID =
      'set_google_photos_daily_refresh_album_id',
  SET_GOOGLE_PHOTOS_ENABLED = 'set_google_photos_enabled',
  SET_IMAGES_FOR_COLLECTION = 'set_images_for_collection',
  SET_DEFAULT_IMAGE_THUMBNAIL = 'set_default_image',
  SET_LOCAL_IMAGES = 'set_local_images',
  SET_LOCAL_IMAGE_DATA = 'set_local_image_data',
  SET_SELECTED_IMAGE = 'set_selected_image',
  SET_UPDATED_DAILY_REFRESH_IMAGE = 'set_updated_daily_refreshed_image',
  SET_FULLSCREEN_ENABLED = 'set_fullscreen_enabled',
}

export type WallpaperActions =
    AppendGooglePhotosAlbumAction|AppendGooglePhotosAlbumsAction|
    AppendGooglePhotosPhotosAction|BeginLoadDefaultImageThumbnailAction|
    BeginLoadGooglePhotosAlbumAction|BeginLoadGooglePhotosAlbumsAction|
    BeginLoadGooglePhotosEnabledAction|BeginLoadGooglePhotosPhotosAction|
    BeginLoadImagesForCollectionsAction|BeginLoadLocalImagesAction|
    BeginLoadLocalImageDataAction|BeginUpdateDailyRefreshImageAction|
    BeginLoadSelectedImageAction|BeginSelectImageAction|ClearDailyRefreshAction|
    EndSelectImageAction|SetCollectionsAction|SetDailyRefreshCollectionIdAction|
    SetGooglePhotosDailyRefreshAlbumIdAction|SetGooglePhotosEnabledAction|
    SetImagesForCollectionAction|SetDefaultImageThumbnailAction|
    SetLocalImageDataAction|SetLocalImagesAction|
    SetUpdatedDailyRefreshImageAction|SetSelectedImageAction|
    SetFullscreenEnabledAction;

export type AppendGooglePhotosAlbumAction = Action&{
  name: WallpaperActionName.APPEND_GOOGLE_PHOTOS_ALBUM,
  albumId: string,
  photos: GooglePhotosPhoto[] | null,
  resumeToken: string | null,
};

/**
 * Appends to the list of Google Photos photos for the album associated with the
 * specified id. May be called with null on error.
 */
export function appendGooglePhotosAlbumAction(
    albumId: string, photos: GooglePhotosPhoto[]|null,
    resumeToken: string|null): AppendGooglePhotosAlbumAction {
  return {
    albumId,
    photos,
    resumeToken,
    name: WallpaperActionName.APPEND_GOOGLE_PHOTOS_ALBUM,
  };
}

export type AppendGooglePhotosAlbumsAction = Action&{
  name: WallpaperActionName.APPEND_GOOGLE_PHOTOS_ALBUMS,
  albums: GooglePhotosAlbum[] | null,
  resumeToken: string | null,
};

/**
 * Appends to the list of Google Photos albums. May be called with null on
 * error.
 */
export function appendGooglePhotosAlbumsAction(
    albums: GooglePhotosAlbum[]|null,
    resumeToken: string|null): AppendGooglePhotosAlbumsAction {
  return {
    albums,
    resumeToken,
    name: WallpaperActionName.APPEND_GOOGLE_PHOTOS_ALBUMS,
  };
}

export type AppendGooglePhotosPhotosAction = Action&{
  name: WallpaperActionName.APPEND_GOOGLE_PHOTOS_PHOTOS,
  photos: GooglePhotosPhoto[] | null,
  resumeToken: string | null,
};

/**
 * Appends to the list of Google Photos photos. May be called with null on
 * error.
 */
export function appendGooglePhotosPhotosAction(
    photos: GooglePhotosPhoto[]|null,
    resumeToken: string|null): AppendGooglePhotosPhotosAction {
  return {
    photos,
    resumeToken,
    name: WallpaperActionName.APPEND_GOOGLE_PHOTOS_PHOTOS,
  };
}

export type BeginLoadGooglePhotosAlbumAction = Action&{
  name: WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM,
  albumId: string,
};

/**
 * Notify that the app is loading the list of Google Photos photos for the album
 * associated with the specified id.
 */
export function beginLoadGooglePhotosAlbumAction(albumId: string):
    BeginLoadGooglePhotosAlbumAction {
  return {albumId, name: WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM};
}

export type BeginLoadGooglePhotosAlbumsAction = Action&{
  name: WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS,
};

/**
 * Notify that the app is loading the list of Google Photos albums.
 */
export function beginLoadGooglePhotosAlbumsAction():
    BeginLoadGooglePhotosAlbumsAction {
  return {name: WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS};
}

export type BeginLoadGooglePhotosEnabledAction = Action&{
  name: WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ENABLED,
};

/**
 * Notify that the app is loading whether the user is allowed to access Google
 * Photos.
 */
export function beginLoadGooglePhotosEnabledAction():
    BeginLoadGooglePhotosEnabledAction {
  return {name: WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ENABLED};
}

export type BeginLoadGooglePhotosPhotosAction = Action&{
  name: WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS,
};

/**
 * Notify that the app is loading the list of Google Photos photos.
 */
export function beginLoadGooglePhotosPhotosAction():
    BeginLoadGooglePhotosPhotosAction {
  return {name: WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS};
}

export type BeginLoadImagesForCollectionsAction = Action&{
  name: WallpaperActionName.BEGIN_LOAD_IMAGES_FOR_COLLECTIONS,
  collections: WallpaperCollection[],
};

/**
 * Notify that app is loading image list for the given collection.
 */
export function beginLoadImagesForCollectionsAction(
    collections: WallpaperCollection[]): BeginLoadImagesForCollectionsAction {
  return {
    collections,
    name: WallpaperActionName.BEGIN_LOAD_IMAGES_FOR_COLLECTIONS,
  };
}

export type BeginLoadDefaultImageThumbnailAction = Action&{
  name: WallpaperActionName.BEGIN_LOAD_DEFAULT_IMAGE_THUMBNAIL,
};

export function beginLoadDefaultImageThubmnailAction():
    BeginLoadDefaultImageThumbnailAction {
  return {name: WallpaperActionName.BEGIN_LOAD_DEFAULT_IMAGE_THUMBNAIL};
}

export type BeginLoadLocalImagesAction = Action&{
  name: WallpaperActionName.BEGIN_LOAD_LOCAL_IMAGES,
};

/**
 * Notify that app is loading local image list.
 */
export function beginLoadLocalImagesAction(): BeginLoadLocalImagesAction {
  return {name: WallpaperActionName.BEGIN_LOAD_LOCAL_IMAGES};
}

export type BeginLoadLocalImageDataAction = Action&{
  name: WallpaperActionName.BEGIN_LOAD_LOCAL_IMAGE_DATA,
  id: string,
};

/**
 * Notify that app is loading thumbnail for the given local image.
 */
export function beginLoadLocalImageDataAction(image: FilePath):
    BeginLoadLocalImageDataAction {
  return {
    id: image.path,
    name: WallpaperActionName.BEGIN_LOAD_LOCAL_IMAGE_DATA,
  };
}

export type BeginUpdateDailyRefreshImageAction = Action&{
  name: WallpaperActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE,
};

/**
 * Notify that a user has clicked on the refresh button.
 */
export function beginUpdateDailyRefreshImageAction():
    BeginUpdateDailyRefreshImageAction {
  return {
    name: WallpaperActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE,
  };
}

export type BeginLoadSelectedImageAction = Action&{
  name: WallpaperActionName.BEGIN_LOAD_SELECTED_IMAGE,
};

/**
 * Notify that app is loading currently selected image information.
 */
export function beginLoadSelectedImageAction(): BeginLoadSelectedImageAction {
  return {name: WallpaperActionName.BEGIN_LOAD_SELECTED_IMAGE};
}

export type BeginSelectImageAction = Action&{
  name: WallpaperActionName.BEGIN_SELECT_IMAGE,
  image: DisplayableImage,
};

/**
 * Notify that a user has clicked on an image to set as wallpaper.
 */
export function beginSelectImageAction(image: DisplayableImage):
    BeginSelectImageAction {
  return {name: WallpaperActionName.BEGIN_SELECT_IMAGE, image};
}

export type EndSelectImageAction = Action&{
  name: WallpaperActionName.END_SELECT_IMAGE,
  image: DisplayableImage,
  success: boolean,
};

/**
 * Notify that the user-initiated action to set image has finished.
 */
export function endSelectImageAction(
    image: DisplayableImage, success: boolean): EndSelectImageAction {
  return {name: WallpaperActionName.END_SELECT_IMAGE, image, success};
}

export type SetCollectionsAction = Action&{
  name: WallpaperActionName.SET_COLLECTIONS,
  collections: WallpaperCollection[] | null,
};

/**
 * Set the collections. May be called with null if an error occurred.
 */
export function setCollectionsAction(collections: WallpaperCollection[]|
                                     null): SetCollectionsAction {
  return {
    name: WallpaperActionName.SET_COLLECTIONS,
    collections,
  };
}

export type SetDailyRefreshCollectionIdAction = Action&{
  name: WallpaperActionName.SET_DAILY_REFRESH_COLLECTION_ID,
  collectionId: string,
};

/**
 * Set and enable daily refresh for given collectionId.
 */
export function setDailyRefreshCollectionIdAction(collectionId: string):
    SetDailyRefreshCollectionIdAction {
  return {
    collectionId,
    name: WallpaperActionName.SET_DAILY_REFRESH_COLLECTION_ID,
  };
}

export type SetGooglePhotosDailyRefreshAlbumIdAction = Action&{
  name: WallpaperActionName.SET_GOOGLE_PHOTOS_DAILY_REFRESH_ALBUM_ID,
  albumId: string,
};

/**
 * Set and enable daily refresh for given Google Photos albumId.
 */
export function setGooglePhotosDailyRefreshAlbumIdAction(albumId: string):
    SetGooglePhotosDailyRefreshAlbumIdAction {
  return {
    albumId,
    name: WallpaperActionName.SET_GOOGLE_PHOTOS_DAILY_REFRESH_ALBUM_ID,
  };
}

export type ClearDailyRefreshAction = Action&{
  name: WallpaperActionName.CLEAR_DAILY_REFRESH_ACTION,
};

/**
 * Clear the data related to daily refresh, indicating daily refresh is not
 * active.
 */
export function clearDailyRefreshAction(): ClearDailyRefreshAction {
  return {
    name: WallpaperActionName.CLEAR_DAILY_REFRESH_ACTION,
  };
}

export type SetGooglePhotosEnabledAction = Action&{
  name: WallpaperActionName.SET_GOOGLE_PHOTOS_ENABLED,
  enabled: GooglePhotosEnablementState,
};

/** Sets whether the user is allowed to access Google Photos. */
export function setGooglePhotosEnabledAction(
    enabled: GooglePhotosEnablementState): SetGooglePhotosEnabledAction {
  return {enabled, name: WallpaperActionName.SET_GOOGLE_PHOTOS_ENABLED};
}

export type SetImagesForCollectionAction = Action&{
  name: WallpaperActionName.SET_IMAGES_FOR_COLLECTION,
  collectionId: string,
  images: WallpaperImage[] | null,
};

/**
 * Set the images for a given collection. May be called with null if an error
 * occurred.
 */
export function setImagesForCollectionAction(
    collectionId: string,
    images: WallpaperImage[]|null): SetImagesForCollectionAction {
  return {
    collectionId,
    images,
    name: WallpaperActionName.SET_IMAGES_FOR_COLLECTION,
  };
}

export type SetDefaultImageThumbnailAction = Action&{
  name: WallpaperActionName.SET_DEFAULT_IMAGE_THUMBNAIL,
  thumbnail: Url,
};

export function setDefaultImageThumbnailAction(thumbnail: Url):
    SetDefaultImageThumbnailAction {
  return {
    thumbnail,
    name: WallpaperActionName.SET_DEFAULT_IMAGE_THUMBNAIL,
  };
}

export type SetLocalImageDataAction = Action&{
  name: WallpaperActionName.SET_LOCAL_IMAGE_DATA,
  id: string,
  data: Url,
};

/**
 * Set the thumbnail data for a local image.
 */
export function setLocalImageDataAction(
    filePath: FilePath, data: Url): SetLocalImageDataAction {
  return {
    id: filePath.path,
    data,
    name: WallpaperActionName.SET_LOCAL_IMAGE_DATA,
  };
}

export type SetLocalImagesAction = Action&{
  name: WallpaperActionName.SET_LOCAL_IMAGES,
  images: FilePath[] | null,
};

/** Set the list of local images. */
export function setLocalImagesAction(images: FilePath[]|
                                     null): SetLocalImagesAction {
  return {
    images,
    name: WallpaperActionName.SET_LOCAL_IMAGES,
  };
}

export type SetUpdatedDailyRefreshImageAction = Action&{
  name: WallpaperActionName.SET_UPDATED_DAILY_REFRESH_IMAGE,
};

/**
 * Notify that a image has been refreshed.
 */
export function setUpdatedDailyRefreshImageAction():
    SetUpdatedDailyRefreshImageAction {
  return {
    name: WallpaperActionName.SET_UPDATED_DAILY_REFRESH_IMAGE,
  };
}

/**
 * |image| may be null if no wallpaper is currently selected or an error
 * occurred while fetching the image. This is rare but can still occur in some
 * scenarios where no wallpaper has ever been selected and no default wallpaper
 * is applied, for example.
 */
export type SetSelectedImageAction = Action&{
  name: WallpaperActionName.SET_SELECTED_IMAGE,
  image: CurrentWallpaper | null,
};

/**
 * Returns an action to set the current image as currently selected across the
 * app. Can be called with null to represent no image currently selected or that
 * an error occurred.
 */
export function setSelectedImageAction(image: CurrentWallpaper|
                                       null): SetSelectedImageAction {
  return {
    image,
    name: WallpaperActionName.SET_SELECTED_IMAGE,
  };
}



export type SetFullscreenEnabledAction = Action&{
  name: WallpaperActionName.SET_FULLSCREEN_ENABLED,
  enabled: boolean,
};

/**
 * Enable/disable the fullscreen preview mode for wallpaper.
 */
export function setFullscreenEnabledAction(enabled: boolean):
    SetFullscreenEnabledAction {
  assert(typeof enabled === 'boolean');
  return {name: WallpaperActionName.SET_FULLSCREEN_ENABLED, enabled};
}
