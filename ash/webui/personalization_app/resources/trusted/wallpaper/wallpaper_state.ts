// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {CurrentWallpaper, GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, WallpaperCollection, WallpaperImage} from '../personalization_app.mojom-webui.js';

/**
 * Stores collections and images from backdrop server.
 * |images| is a mapping of collection id to the list of images.
 */
export interface BackdropState {
  collections: Array<WallpaperCollection>|null;
  images: Record<WallpaperCollection['id'], Array<WallpaperImage>|null>;
}

/**
 * Stores Google Photos state.
 * |enabled| is whether the user is allowed to access Google Photos. It is
 * undefined only until it has been initialized.
 * |count| is the count of Google Photos photos. It is undefined only until it
 * has been initialized, then either null (in error state) or a valid integer.
 * |albums| is the list of Google Photos albums. It is undefined only until it
 * has been initialized, then either null (in error state) or a valid Array.
 * |photos| is the list of Google Photos photos. It is undefined only until it
 * has been initialized, then either null (in error state) or a valid Array.
 * |photosByAlbumId| is the list of Google Photos photos keyed by album id. The
 * list of photos for a given album id is undefined only until is has been
 * initialized, then either null (in error state) or a valid Array.
 */
export interface GooglePhotosState {
  enabled: GooglePhotosEnablementState|undefined;
  count: number|null|undefined;
  albums: GooglePhotosAlbum[]|null|undefined;
  photos: GooglePhotosPhoto[]|null|undefined;
  photosByAlbumId: Record<string, GooglePhotosPhoto[]|null|undefined>;
  resumeTokens: {
    albums: string|null,
    photos: string|null,
    photosByAlbumId: Record<string, string|null>,
  };
}

/**
 * Stores loading state of various components of the app.
 * |images| is a mapping of collection id to loading state.
 * |local| stores data just for local images on disk.
 * |local.data| stores a mapping of FilePath.path string to loading state.
 *
 * |selected| is a boolean representing the loading state of current wallpaper
 * information. This gets complicated when a user rapidly selects multiple
 * wallpaper images, or picks a new daily refresh wallpaper. This becomes
 * false when a new CurrentWallpaper object is received and the |setImage|
 * counter is at 0.
 *
 * |setImage| is a number representing the number of concurrent requests to set
 * current wallpaper information. This can be more than 1 in case a user rapidly
 * selects multiple wallpaper options.
 *
 * |googlePhotos| stores loading state of Google Photos data.
 */
export interface LoadingState {
  collections: boolean;
  images: Record<WallpaperCollection['id'], boolean>;
  local: {images: boolean, data: Record<FilePath['path'], boolean>};
  refreshWallpaper: boolean;
  selected: boolean;
  setImage: number;
  googlePhotos: {
    enabled: boolean,
    count: boolean,
    albums: boolean,
    photos: boolean,
    photosByAlbumId: Record<string, boolean>,
  };
}

/**
 * |images| stores the list of images on local disk.
 * |data| stores a mapping of image.path to a thumbnail data url.
 */
export interface LocalState {
  images: Array<FilePath>|null;
  data: Record<FilePath['path'], string>;
}

export interface DailyRefreshState {
  collectionId: string|null;
}

export interface WallpaperState {
  backdrop: BackdropState;
  loading: LoadingState;
  local: LocalState;
  currentSelected: CurrentWallpaper|null;
  pendingSelected: WallpaperImage|FilePath|GooglePhotosPhoto|null;
  dailyRefresh: DailyRefreshState;
  fullscreen: boolean;
  googlePhotos: GooglePhotosState;
}

export function emptyState(): WallpaperState {
  return {
    backdrop: {collections: null, images: {}},
    loading: {
      collections: true,
      images: {},
      local: {images: false, data: {}},
      refreshWallpaper: false,
      selected: false,
      setImage: 0,
      googlePhotos: {
        enabled: false,
        count: false,
        albums: false,
        photos: false,
        photosByAlbumId: {},
      },
    },
    local: {images: null, data: {}},
    currentSelected: null,
    pendingSelected: null,
    dailyRefresh: {collectionId: null},
    fullscreen: false,
    googlePhotos: {
      enabled: undefined,
      count: undefined,
      albums: undefined,
      photos: undefined,
      photosByAlbumId: {},
      resumeTokens: {albums: null, photos: null, photosByAlbumId: {}},
    },
  };
}
