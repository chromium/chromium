// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {Actions} from '../personalization_actions.js';
import {WallpaperCollection} from '../personalization_app.mojom-webui.js';
import {ReducerFunction} from '../personalization_reducers.js';
import {PersonalizationState} from '../personalization_state.js';
import {isImageDataUrl, isNonEmptyArray} from '../utils.js';

import {DefaultImageSymbol, kDefaultImageSymbol} from './constants.js';
import {isDefaultImage, isFilePath} from './utils.js';
import {WallpaperActionName} from './wallpaper_actions.js';
import {DailyRefreshType, WallpaperState} from './wallpaper_state.js';

function backdropReducer(
    state: WallpaperState['backdrop'], action: Actions,
    _: PersonalizationState): WallpaperState['backdrop'] {
  switch (action.name) {
    case WallpaperActionName.SET_COLLECTIONS:
      return {collections: action.collections, images: {}};
    case WallpaperActionName.SET_IMAGES_FOR_COLLECTION:
      if (!state.collections) {
        console.warn('Cannot set images when collections is null');
        return state;
      }
      if (!state.collections.some(({id}) => id === action.collectionId)) {
        console.warn(
            'Cannot store images for unknown collection', action.collectionId);
        return state;
      }
      return {
        ...state,
        images: {...state.images, [action.collectionId]: action.images},
      };
    default:
      return state;
  }
}

function loadingReducer(
    state: WallpaperState['loading'], action: Actions,
    _: PersonalizationState): WallpaperState['loading'] {
  switch (action.name) {
    case WallpaperActionName.BEGIN_LOAD_IMAGES_FOR_COLLECTIONS:
      return {
        ...state,
        images: action.collections.reduce(
            (result, {id}) => {
              result[id] = true;
              return result;
            },
            {} as Record<WallpaperCollection['id'], boolean>),
      };
    case WallpaperActionName.BEGIN_LOAD_LOCAL_IMAGE_DATA:
      return {
        ...state,
        local: {...state.local, data: {...state.local.data, [action.id]: true}},
      };
    case WallpaperActionName.BEGIN_LOAD_SELECTED_IMAGE:
      return {...state, selected: true};
    case WallpaperActionName.BEGIN_SELECT_IMAGE:
      return {...state, setImage: state.setImage + 1};
    case WallpaperActionName.END_SELECT_IMAGE:
      if (state.setImage <= 0) {
        console.error('Impossible state for loading.setImage');
        // Reset to 0.
        return {...state, setImage: 0};
      }
      return {...state, setImage: state.setImage - 1};
    case WallpaperActionName.SET_COLLECTIONS:
      return {...state, collections: false};
    case WallpaperActionName.SET_IMAGES_FOR_COLLECTION:
      return {
        ...state,
        images: {...state.images, [action.collectionId]: false},
      };
    case WallpaperActionName.BEGIN_LOAD_DEFAULT_IMAGE_THUMBNAIL:
      return {
        ...state,
        local: {
          ...state.local,
          data: {
            ...state.local.data,
            [kDefaultImageSymbol]: true,
          },
        },
      };
    case WallpaperActionName.BEGIN_LOAD_LOCAL_IMAGES:
      return {
        ...state,
        local: {
          ...state.local,
          images: true,
        },
      };
    case WallpaperActionName.SET_DEFAULT_IMAGE_THUMBNAIL:
      return {
        ...state,
        local: {
          ...state.local,
          data: {
            ...state.local.data,
            [kDefaultImageSymbol]: false,
          },
        },
      };
    case WallpaperActionName.SET_LOCAL_IMAGES:
      // Only keep loading state for most recent local images and the default
      // image.
      const imagesToKeep: Array<DefaultImageSymbol|FilePath> =
          [kDefaultImageSymbol, ...(action.images || [])];
      return {
        ...state,
        local: {
          data: imagesToKeep.reduce(
              (result, next) => {
                const path = isFilePath(next) ? next.path : next;
                if (state.local.data.hasOwnProperty(path)) {
                  result[path] = state.local.data[path];
                }
                return result;
              },
              {} as Record<FilePath['path']|DefaultImageSymbol, boolean>),
          // Image list is done loading.
          images: false,
        },
      };
    case WallpaperActionName.SET_LOCAL_IMAGE_DATA:
      return {
        ...state,
        local: {
          ...state.local,
          data: {
            ...state.local.data,
            [action.id]: false,
          },
        },
      };
    case WallpaperActionName.SET_SELECTED_IMAGE:
      if (state.setImage === 0) {
        return {...state, selected: false};
      }
      return state;
    case WallpaperActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE:
      return {...state, refreshWallpaper: true};
    case WallpaperActionName.SET_UPDATED_DAILY_REFRESH_IMAGE:
      return {...state, refreshWallpaper: false};
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM:
      assert(!state.googlePhotos.photosByAlbumId[action.albumId]);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          photosByAlbumId: {
            ...state.googlePhotos.photosByAlbumId,
            [action.albumId]: true,
          },
        },
      };
    case WallpaperActionName.APPEND_GOOGLE_PHOTOS_ALBUM:
      assert(state.googlePhotos.photosByAlbumId[action.albumId] === true);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          photosByAlbumId: {
            ...state.googlePhotos.photosByAlbumId,
            [action.albumId]: false,
          },
        },
      };
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS:
      assert(state.googlePhotos.albums === false);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          albums: true,
        },
      };
    case WallpaperActionName.APPEND_GOOGLE_PHOTOS_ALBUMS:
      assert(state.googlePhotos.albums === true);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          albums: false,
        },
      };
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ENABLED:
      assert(state.googlePhotos.enabled === false);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          enabled: true,
        },
      };
    case WallpaperActionName.SET_GOOGLE_PHOTOS_ENABLED:
      assert(state.googlePhotos.enabled === true);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          enabled: false,
        },
      };
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS:
      assert(state.googlePhotos.photos === false);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          photos: true,
        },
      };
    case WallpaperActionName.APPEND_GOOGLE_PHOTOS_PHOTOS:
      assert(state.googlePhotos.photos === true);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          photos: false,
        },
      };
    default:
      return state;
  }
}

function localReducer(
    state: WallpaperState['local'], action: Actions,
    _: PersonalizationState): WallpaperState['local'] {
  switch (action.name) {
    case WallpaperActionName.SET_DEFAULT_IMAGE_THUMBNAIL:
      if (isImageDataUrl(action.thumbnail)) {
        return {
          images: [
            kDefaultImageSymbol,
            ...(state.images || []).filter(img => isFilePath(img)),
          ],
          data: {
            ...state.data,
            [kDefaultImageSymbol]: action.thumbnail,
          },
        };
      }
      return {
        images: Array.isArray(state.images) ?
            state.images.filter(img => isFilePath(img)) :
            null,
        data: {...state.data, [kDefaultImageSymbol]: {url: ''}},
      };

    case WallpaperActionName.SET_LOCAL_IMAGES: {
      const hasDefaultImageWithData = isNonEmptyArray(state.images) &&
          isDefaultImage(state.images[0]) && !!state.data[kDefaultImageSymbol];

      if (!Array.isArray(action.images)) {
        return {
          // Keep the default image in image list if it is present.
          images: hasDefaultImageWithData ? [kDefaultImageSymbol] : null,
          data: {[kDefaultImageSymbol]: state.data[kDefaultImageSymbol]},
        };
      }
      // If the first image from prior state is the device default image, keep
      // it.
      const newImages: Array<DefaultImageSymbol|FilePath> =
          hasDefaultImageWithData ? [kDefaultImageSymbol, ...action.images] :
                                    action.images;
      return {
        images: newImages,
        // Only keep image thumbnails if the image is still in |images|.
        data: newImages.reduce(
            (result, next) => {
              const key = isFilePath(next) ? next.path : next;
              if (state.data.hasOwnProperty(key)) {
                result[key] = state.data[key];
              }
              return result;
            },
            // Set the default value for |kDefaultImageSymbol| here.
            {[kDefaultImageSymbol]: {url: ''}} as typeof state.data),
      };
    }
    case WallpaperActionName.SET_LOCAL_IMAGE_DATA:
      return {
        ...state,
        data: {
          ...state.data,
          [action.id]: action.data,
        },
      };
    default:
      return state;
  }
}

function currentSelectedReducer(
    state: WallpaperState['currentSelected'], action: Actions,
    _: PersonalizationState): WallpaperState['currentSelected'] {
  switch (action.name) {
    case WallpaperActionName.SET_SELECTED_IMAGE:
      return action.image;
    default:
      return state;
  }
}

/**
 * Reducer for the pending selected image. The pendingSelected state is set when
 * a user clicks on an image and before the client code is reached.
 *
 * Note: We allow multiple concurrent requests of selecting images while only
 * keeping the latest pending image and failing others occurred in between.
 * The pendingSelected state should not be cleared in this scenario (of multiple
 * concurrent requests). Otherwise, it results in a unwanted jumpy motion of
 * selected state.
 */
function pendingSelectedReducer(
    state: WallpaperState['pendingSelected'], action: Actions,
    globalState: PersonalizationState): WallpaperState['pendingSelected'] {
  switch (action.name) {
    case WallpaperActionName.BEGIN_SELECT_IMAGE:
      return action.image;
    case WallpaperActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE:
      return null;
    case WallpaperActionName.SET_SELECTED_IMAGE:
      const {image} = action;
      if (!image) {
        console.warn('pendingSelectedReducer: Failed to get selected image.');
        return null;
      } else if (globalState.wallpaper.loading.setImage == 0) {
        // Clear the pending state when there are no more requests.
        return null;
      }
      return state;
    case WallpaperActionName.SET_FULLSCREEN_ENABLED:
      if (!action.enabled) {
        // Clear the pending selected state after full screen is dismissed.
        return null;
      }
      return state;
    case WallpaperActionName.END_SELECT_IMAGE:
      const {success} = action;
      if (!success && globalState.wallpaper.loading.setImage <= 1) {
        // Clear the pending selected state if an error occurs and
        // there are no multiple concurrent requests of selecting images.
        return null;
      }
      return state;
    default:
      return state;
  }
}

function dailyRefreshReducer(
    state: WallpaperState['dailyRefresh'], action: Actions,
    _: PersonalizationState): WallpaperState['dailyRefresh'] {
  switch (action.name) {
    case WallpaperActionName.SET_DAILY_REFRESH_COLLECTION_ID:
      return {
        id: action.collectionId,
        type: DailyRefreshType.BACKDROP,
      };
    case WallpaperActionName.SET_GOOGLE_PHOTOS_DAILY_REFRESH_ALBUM_ID:
      return {
        id: action.albumId,
        type: DailyRefreshType.GOOGLE_PHOTOS,
      };
    case WallpaperActionName.CLEAR_DAILY_REFRESH_ACTION:
      return null;
    default:
      return state;
  }
}


function fullscreenReducer(
    state: WallpaperState['fullscreen'], action: Actions,
    _: PersonalizationState): WallpaperState['fullscreen'] {
  switch (action.name) {
    case WallpaperActionName.SET_FULLSCREEN_ENABLED:
      return action.enabled;
    default:
      return state;
  }
}

function googlePhotosReducer(
    state: WallpaperState['googlePhotos'], action: Actions,
    _: PersonalizationState): WallpaperState['googlePhotos'] {
  switch (action.name) {
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM:
      // The list of photos for an album should be loaded only while additional
      // photos exist.
      assert(!!state.albums);
      assert(state.albums.some(album => album.id === action.albumId));
      assert(
          !state.photosByAlbumId[action.albumId] ||
          state.resumeTokens.photosByAlbumId[action.albumId]);
      return state;
    case WallpaperActionName.APPEND_GOOGLE_PHOTOS_ALBUM:
      assert(!!state.albums);
      assert(state.albums.some(album => album.id === action.albumId));
      assert(action.albumId !== undefined);
      assert(action.photos !== undefined);
      // Case: First batch of photos.
      if (!Array.isArray(state.photosByAlbumId[action.albumId])) {
        return {
          ...state,
          photosByAlbumId: {
            ...state.photosByAlbumId,
            [action.albumId]: action.photos,
          },
          resumeTokens: {
            ...state.resumeTokens,
            photosByAlbumId: {
              ...state.resumeTokens.photosByAlbumId,
              [action.albumId]: action.resumeToken,
            },
          },
        };
      }
      // Case: Subsequent batches of photos.
      if (Array.isArray(action.photos)) {
        return {
          ...state,
          photosByAlbumId: {
            ...state.photosByAlbumId,
            [action.albumId]:
                [...state.photosByAlbumId[action.albumId]!, ...action.photos],
          },
          resumeTokens: {
            ...state.resumeTokens,
            photosByAlbumId: {
              ...state.resumeTokens.photosByAlbumId,
              [action.albumId]: action.resumeToken,
            },
          },
        };
      }
      // Case: Error.
      return {
        ...state,
        resumeTokens: {
          ...state.resumeTokens,
          photosByAlbumId: {
            ...state.resumeTokens.photosByAlbumId,
            [action.albumId]: action.resumeToken,
          },
        },
      };
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS:
      // The list of albums should be loaded only while additional albums exist.
      assert(!state.albums || state.resumeTokens.albums);
      return state;
    case WallpaperActionName.APPEND_GOOGLE_PHOTOS_ALBUMS:
      assert(action.albums !== undefined);
      // Case: First batch of albums.
      if (!Array.isArray(state.albums)) {
        return {
          ...state,
          albums: action.albums,
          resumeTokens: {
            ...state.resumeTokens,
            albums: action.resumeToken,
          },
        };
      }
      // Case: Subsequent batches of albums.
      if (Array.isArray(action.albums)) {
        return {
          ...state,
          albums: [...state.albums, ...action.albums],
          resumeTokens: {
            ...state.resumeTokens,
            albums: action.resumeToken,
          },
        };
      }
      // Case: Error.
      return {
        ...state,
        resumeTokens: {
          ...state.resumeTokens,
          albums: action.resumeToken,
        },
      };
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ENABLED:
      // Whether the user is allowed to access Google Photos should be loaded
      // only once.
      assert(state.enabled === undefined);
      return state;
    case WallpaperActionName.SET_GOOGLE_PHOTOS_ENABLED:
      assert(action.enabled !== undefined);
      return {
        ...state,
        enabled: action.enabled,
      };
    case WallpaperActionName.BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS:
      // The list of photos should be loaded only while additional photos exist.
      assert(!state.photos || state.resumeTokens.photos);
      return state;
    case WallpaperActionName.APPEND_GOOGLE_PHOTOS_PHOTOS:
      assert(action.photos !== undefined);
      // Case: First batch of photos.
      if (!Array.isArray(state.photos)) {
        return {
          ...state,
          photos: action.photos,
          resumeTokens: {
            ...state.resumeTokens,
            photos: action.resumeToken,
          },
        };
      }
      // Case: Subsequent batches of photos.
      if (Array.isArray(action.photos)) {
        return {
          ...state,
          photos: [...state.photos, ...action.photos],
          resumeTokens: {
            ...state.resumeTokens,
            photos: action.resumeToken,
          },
        };
      }
      // Case: Error.
      return {
        ...state,
        resumeTokens: {
          ...state.resumeTokens,
          photos: action.resumeToken,
        },
      };
    default:
      return state;
  }
}

export const wallpaperReducers:
    {[K in keyof WallpaperState]: ReducerFunction<WallpaperState[K]>} = {
      backdrop: backdropReducer,
      loading: loadingReducer,
      local: localReducer,
      currentSelected: currentSelectedReducer,
      pendingSelected: pendingSelectedReducer,
      dailyRefresh: dailyRefreshReducer,
      fullscreen: fullscreenReducer,
      googlePhotos: googlePhotosReducer,
    };
