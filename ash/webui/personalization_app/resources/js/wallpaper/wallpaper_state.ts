// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FullscreenPreviewState} from 'chrome://resources/ash/common/personalization/wallpaper_state.js';
import {emptyState as emptySeaPenState, SeaPenState} from 'chrome://resources/ash/common/sea_pen/sea_pen_state.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {CurrentAttribution, CurrentWallpaper, GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, WallpaperCollection, WallpaperImage} from '../../personalization_app.mojom-webui.js';

import {DefaultImageSymbol, DisplayableImage, kDefaultImageSymbol} from './constants.js';

/**
 * Stores collections and images from backdrop server.
 * |images| is a mapping of collection id to the list of images.
 */
export interface BackdropState {
  collections: WallpaperCollection[]|null;
  images: Record<WallpaperCollection['id'], WallpaperImage[]|null>;
}

/**
 * Stores Google Photos state.
 * |enabled| is whether the user is allowed to access Google Photos. It is
 * undefined only until it has been initialized.
 * |albums| is the list of Google Photos owned albums. It is undefined only
 * until it has been initialized, then either null (in error state) or a valid
 * Array.
 * |albumsShared| is the list of Google Photos shared albums.
 * |photos| is the list of Google Photos photos. It is undefined only until it
 * has been initialized, then either null (in error state) or a valid Array.
 * |photosByAlbumId| is the list of Google Photos photos keyed by album id. The
 * list of photos for a given album id is undefined only until is has been
 * initialized, then either null (in error state) or a valid Array.
 */
export interface GooglePhotosState {
  enabled: GooglePhotosEnablementState|undefined;
  albums: GooglePhotosAlbum[]|null|undefined;
  albumsShared: GooglePhotosAlbum[]|null|undefined;
  photos: GooglePhotosPhoto[]|null|undefined;
  photosByAlbumId: Record<string, GooglePhotosPhoto[]|null|undefined>;
  resumeTokens: {
    albums: string|null,
    albumsShared: string|null,
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
 * |selected| stores the loading state of current wallpaper image and
 * attribution. This gets complicated when a user rapidly selects multiple
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
  local: {
    images: boolean,
    data: Record<FilePath['path']|DefaultImageSymbol, boolean>,
  };
  refreshWallpaper: boolean;
  selected: {
    attribution: boolean,
    image: boolean,
  };
  setImage: number;
  googlePhotos: {
    enabled: boolean,
    albums: boolean,
    albumsShared: boolean,
    photos: boolean,
    photosByAlbumId: Record<string, boolean>,
  };
}

/**
 * |images| stores the list of images on local disk. The image in index 0 may be
 * a special case for default image thumbnail.
 * |data| stores a mapping of image.path to a thumbnail data url. There is also
 * a special key to represent the default image thumbnail.
 */
export interface LocalState {
  images: Array<FilePath|DefaultImageSymbol>|null;
  data: Record<FilePath['path']|DefaultImageSymbol, Url>;
}

export enum DailyRefreshType {
  GOOGLE_PHOTOS = 'daily_refresh_google_photos',
  BACKDROP = 'daily_refresh_backdrop',
}

/**
 * |id| stores either a Backdrop collection id or a Google Photos album id.
 * |type| stores which type of daily refresh and type of id this is.
 */
export interface DailyRefreshState {
  id: string;
  type: DailyRefreshType;
}

export interface WallpaperState {
  backdrop: BackdropState;
  loading: LoadingState;
  local: LocalState;
  attribution: CurrentAttribution|null;
  currentSelected: CurrentWallpaper|null;
  pendingSelected: DisplayableImage|null;
  dailyRefresh: DailyRefreshState|null;
  fullscreen: FullscreenPreviewState;
  shouldShowTimeOfDayWallpaperDialog: boolean;
  googlePhotos: GooglePhotosState;
  seaPen: SeaPenState;
}

export function emptyState(): WallpaperState {
  return {
    backdrop: {collections: null, images: {}},
    loading: {
      collections: true,
      images: {},
      local: {images: false, data: {[kDefaultImageSymbol]: false}},
      refreshWallpaper: false,
      selected: {
        attribution: false,
        image: false,
      },
      setImage: 0,
      googlePhotos: {
        enabled: false,
        albums: false,
        albumsShared: false,
        photos: false,
        photosByAlbumId: {},
      },
    },
    local: {images: null, data: {[kDefaultImageSymbol]: {url: ''}}},
    attribution: null,
    currentSelected: null,
    pendingSelected: null,
    dailyRefresh: null,
    fullscreen: FullscreenPreviewState.OFF,
    shouldShowTimeOfDayWallpaperDialog: false,
    googlePhotos: {
      enabled: undefined,
      albums: undefined,
      albumsShared: undefined,
      photos: undefined,
      photosByAlbumId: {},
      resumeTokens:
          {albums: null, albumsShared: null, photos: null, photosByAlbumId: {}},
    },
    seaPen: emptySeaPenState(),
  };
}
