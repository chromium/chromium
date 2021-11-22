// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview  Defines reducers for personalization app.  Reducers must be a
 * pure function that returns a new state object if anything has changed.
 * @see [redux tutorial]{@link https://redux.js.org/tutorials/fundamentals/part-3-state-actions-reducers}
 */

import {assert} from 'chrome://resources/js/assert.m.js';
import {Action} from 'chrome://resources/js/cr/ui/store.js';
import {ActionName} from './personalization_actions.js';

/**
 * @typedef {mojoBase.mojom.FilePath | WallpaperImage}
 */
export let DisplayableImage;

/**
 * Combines reducers into a single top level reducer. Inspired by Redux's
 * |combineReducers| functions.
 * @param {!Object<string, !Function>} mapping
 * @return {function(!PersonalizationState, !Action): !PersonalizationState}
 */
function combineReducers(mapping) {
  /**
   * @param {!PersonalizationState} state
   * @param {!Action} action
   * @return {!PersonalizationState}
   */
  function reduce(state, action) {
    const newState = Object.keys(mapping).reduce((result, key) => {
      const func = mapping[key];
      result[key] = func(state[key], action, state);
      return result;
    }, /** @type {!PersonalizationState} */ ({}));
    const change =
        Object.entries(state).some(([key, value]) => newState[key] !== value);
    return change ? newState : state;
  }
  return reduce;
}

/**
 * @param {!BackdropState} state
 * @param {!Action} action
 * @param {!PersonalizationState} globalState - Top level personalization state,
 * to access specific states when needed (i.e.: loading state).
 * @return {!BackdropState}
 */
function backdropReducer(state, action, globalState) {
  switch (action.name) {
    case ActionName.SET_COLLECTIONS:
      return {collections: action.collections, images: {}};
    case ActionName.SET_IMAGES_FOR_COLLECTION:
      if (!state.collections.some(({id}) => id === action.collectionId)) {
        console.warn(
            'Cannot store images for unknown collection', action.collectionId);
        return state;
      }
      return /** @type {!BackdropState} */ ({
        ...state,
        images: {...state.images, [action.collectionId]: action.images}
      });
    default:
      return state;
  }
}

/**
 * @param {!LoadingState} state
 * @param {!Action} action
 * @param {!PersonalizationState} globalState - Top level personalization state,
 * to access specific states when needed (i.e.: loading state).
 * @return {!LoadingState}
 */
function loadingReducer(state, action, globalState) {
  switch (action.name) {
    case ActionName.BEGIN_LOAD_IMAGES_FOR_COLLECTIONS:
      return /** @type {!LoadingState} */ ({
        ...state,
        images: action.collections.reduce(
            (result, {id}) => {
              result[id] = true;
              return result;
            },
            {})
      });
    case ActionName.BEGIN_LOAD_LOCAL_IMAGE_DATA:
      return /** @type {!LoadingState} */ ({
        ...state,
        local: {...state.local, data: {...state.local.data, [action.id]: true}}
      });
    case ActionName.BEGIN_LOAD_SELECTED_IMAGE:
      return /** @type {!LoadingState} */ ({...state, selected: true});
    case ActionName.BEGIN_SELECT_IMAGE:
      return /** @type {!LoadingState} */ (
          {...state, setImage: state.setImage + 1});
    case ActionName.END_SELECT_IMAGE:
      if (state.setImage <= 0) {
        console.error('Impossible state for loading.setImage');
        // Reset to 0.
        return /** @type {!LoadingState} */ ({...state, setImage: 0});
      }
      return /** @type {!LoadingState} */ (
          {...state, setImage: state.setImage - 1});
    case ActionName.SET_COLLECTIONS:
      return /** @type {!LoadingState} */ ({...state, collections: false});
    case ActionName.SET_IMAGES_FOR_COLLECTION:
      return /** @type {!LoadingState} */ ({
        ...state,
        images: {...state.images, [action.collectionId]: false},
      });
    case ActionName.BEGIN_LOAD_LOCAL_IMAGES:
      return /** @type {!LoadingState} */ ({
        ...state,
        local: {
          ...state.local,
          images: true,
        },
      });
    case ActionName.SET_LOCAL_IMAGES:
      return /** @type {!LoadingState} */ ({
        ...state,
        local: {
          // Only keep loading state for most recent local images.
          data: (action.images || []).reduce(
              (result, {path}) => {
                if (state.local.data.hasOwnProperty(path)) {
                  result[path] = state.local.data[path];
                }
                return result;
              },
              {}),
          // Image list is done loading.
          images: false,
        },
      });
    case ActionName.SET_LOCAL_IMAGE_DATA:
      return /** @type {!LoadingState} */ ({
        ...state,
        local: {
          ...state.local,
          data: {
            ...state.local.data,
            [action.id]: false,
          },
        },
      });
    case ActionName.SET_SELECTED_IMAGE:
      if (state.setImage === 0) {
        return /** @type {!LoadingState} */ ({...state, selected: false});
      }
      return state;
    case ActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE:
      return /** @type {!LoadingState} */ ({...state, refreshWallpaper: true});
    case ActionName.SET_UPDATED_DAILY_REFRESH_IMAGE:
      return /** @type {!LoadingState} */ ({...state, refreshWallpaper: false});
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM:
      assert(state.googlePhotos.photosByAlbumId[action.albumId] === undefined);
      return /** @type {!LoadingState} */ ({
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          photosByAlbumId: {
            ...state.googlePhotos.photosByAlbumId,
            [action.albumId]: true,
          },
        },
      });
    case ActionName.SET_GOOGLE_PHOTOS_ALBUM:
      assert(state.googlePhotos.photosByAlbumId[action.albumId] === true);
      return /** @type {!LoadingState} */ ({
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          photosByAlbumId: {
            ...state.googlePhotos.photosByAlbumId,
            [action.albumId]: false,
          },
        },
      });
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS:
      assert(state.googlePhotos.albums === false);
      return /** @type {!LoadingState} */ ({
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          albums: true,
        },
      });
    case ActionName.SET_GOOGLE_PHOTOS_ALBUMS:
      assert(state.googlePhotos.albums === true);
      return /** @type {!LoadingState} */ ({
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          albums: false,
        },
      });
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_COUNT:
      assert(state.googlePhotos.count === false);
      return /** @type {!LoadingState} */ ({
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          count: true,
        },
      });
    case ActionName.SET_GOOGLE_PHOTOS_COUNT:
      assert(state.googlePhotos.count === true);
      return /** @type {!LoadingState} */ ({
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          count: false,
        },
      });
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS:
      assert(state.googlePhotos.photos === false);
      return /** @type {!LoadingState} */ ({
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          photos: true,
        },
      });
    case ActionName.SET_GOOGLE_PHOTOS_PHOTOS:
      assert(state.googlePhotos.photos === true);
      return /** @type {!LoadingState} */ ({
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          photos: false,
        },
      });
    default:
      return state;
  }
}

/**
 * @param {!LocalState} state
 * @param {!Action} action
 * @param {!PersonalizationState} globalState - Top level personalization state,
 * to access specific states when needed (i.e.: loading state).
 * @return {!LocalState}
 */
function localReducer(state, action, globalState) {
  switch (action.name) {
    case ActionName.SET_LOCAL_IMAGES:
      return /** @type {!LocalState} */ ({
        ...state,
        images: action.images,
        // Only keep image thumbnails if the image is still in |images|.
        data: (action.images || []).reduce(
            (result, {path}) => {
              if (state.data.hasOwnProperty(path)) {
                result[path] = state.data[path];
              }
              return result;
            },
            {}),
      });
    case ActionName.SET_LOCAL_IMAGE_DATA:
      return /** @type {!LocalState} */ ({
        ...state,
        data: {
          ...state.data,
          [action.id]: action.data,
        }
      });
    default:
      return state;
  }
}

/**
 * @param {?CurrentWallpaper} state
 * @param {!Action} action
 * @param {!PersonalizationState} globalState - Top level personalization state,
 * to access specific states when needed (i.e.: loading state).
 * @return {?CurrentWallpaper}
 */
function currentSelectedReducer(state, action, globalState) {
  switch (action.name) {
    case ActionName.SET_SELECTED_IMAGE:
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
 *
 * @param {?DisplayableImage} state
 * @param {!Action} action
 * @param {!PersonalizationState} globalState - Top level personalization state,
 * to access specific states when needed (i.e.: loading state).
 * @return {?DisplayableImage}
 */
function pendingSelectedReducer(state, action, globalState) {
  switch (action.name) {
    case ActionName.BEGIN_SELECT_IMAGE:
      return action.image;
    case ActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE:
      return null;
    case ActionName.SET_SELECTED_IMAGE:
      const {image} = action;
      if (!image) {
        console.warn('pendingSelectedReducer: Failed to get selected image.');
        return null;
      }
      return state;
    case ActionName.SET_FULLSCREEN_ENABLED:
      if (!(/** @type {{enabled: boolean}} */ (action)).enabled) {
        // Clear the pending selected state after full screen is dismissed.
        return null;
      }
      return state;
    case ActionName.END_SELECT_IMAGE:
      const {success} =
          /** @type {{name: string, success: boolean}} */ (action);
      if (!success && globalState.loading.setImage <= 1) {
        // Clear the pending selected state if an error occurs and
        // there are no multiple concurrent requests of selecting images.
        return null;
      }
      return state;
    default:
      return state;
  }
}

/**
 * @param {!DailyRefreshState} state
 * @param {!Action} action
 * @param {!PersonalizationState} globalState - Top level personalization state,
 * to access specific states when needed (i.e.: loading state).
 * @returns {!DailyRefreshState}
 */
function dailyRefreshReducer(state, action, globalState) {
  switch (action.name) {
    case ActionName.SET_DAILY_REFRESH_COLLECTION_ID:
      return /** @type {!DailyRefreshState} */ ({
        ...state,
        collectionId: action.collectionId,
      });
    default:
      return state;
  }
}

/**
 * @param {?string} state
 * @param {!Action} action
 * @param {!PersonalizationState} globalState - Top level personalization state,
 * to access specific states when needed (i.e.: loading state).
 * @return {?string}
 */
function errorReducer(state, action, globalState) {
  switch (action.name) {
    case ActionName.END_SELECT_IMAGE:
      const {success} =
          /** @type {{name: string, success: boolean}} */ (action);
      if (success) {
        return null;
      }
      return state || loadTimeData.getString('setWallpaperError');
    case ActionName.SET_SELECTED_IMAGE:
        const {image} = /** @type {{name: string, image: ?Object}} */(action);
        if (image) {
          return state;
        }
        return state || loadTimeData.getString('loadWallpaperError');
    case ActionName.DISMISS_ERROR:
      if (!state) {
        console.warn(
            'Received dismiss error action when error is already null');
      }
      return null;
    default:
      return state;
  }
}

/**
 * @param {boolean} state
 * @param {!Action} action
 * @param {!PersonalizationState} globalState - Top level personalization state,
 * to access specific states when needed (i.e.: loading state).
 * @return {boolean}
 */
function fullscreenReducer(state, action, globalState) {
  switch (action.name) {
    case ActionName.SET_FULLSCREEN_ENABLED:
      return (/** @type {{enabled: boolean}} */(action)).enabled;
    default:
      return state;
  }
}

/**
 * @param {!GooglePhotosState} state
 * @param {!Action} action
 * @param {!PersonalizationState} globalState - Top level personalization state,
 * to access specific states when needed (i.e.: loading state).
 * @return {!GooglePhotosState}
 */
function googlePhotosReducer(state, action, globalState) {
  switch (action.name) {
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM:
      // The list of photos for an album should be loaded only once.
      assert(state.albums?.some(album => album.id === action.albumId));
      assert(state.photosByAlbumId[action.albumId] === undefined);
      return state;
    case ActionName.SET_GOOGLE_PHOTOS_ALBUM:
      assert(state.albums?.some(album => album.id === action.albumId));
      assert(action.albumId !== undefined);
      assert(action.photos !== undefined);
      return /** @type {!GooglePhotosState} */ ({
        ...state,
        photosByAlbumId: {
          ...state.photosByAlbumId,
          [action.albumId]: action.photos,
        },
      });
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS:
      // The list of albums should be loaded only once.
      assert(state.albums === undefined);
      return state;
    case ActionName.SET_GOOGLE_PHOTOS_ALBUMS:
      assert(action.albums !== undefined);
      return /** @type {!GooglePhotosState} */ ({
        ...state,
        albums: (/** @type {{albums: ?Array<WallpaperCollection>}} */ (action))
                    .albums,
      });
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_COUNT:
      // The total count of photos should be loaded only once.
      assert(state.count === undefined);
      return state;
    case ActionName.SET_GOOGLE_PHOTOS_COUNT:
      assert(action.count !== undefined);
      return /** @type {!GooglePhotosState} */ ({
        ...state,
        count: (/** @type {{count: ?number}} */ (action)).count,
      });
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS:
      // The list of photos should be loaded only once.
      assert(state.photos === undefined);
      return state;
    case ActionName.SET_GOOGLE_PHOTOS_PHOTOS:
      assert(action.photos !== undefined);
      return /** @type {!GooglePhotosState} */ ({
        ...state,
        photos: (/** @type {{photos: ?Array<undefined>}} */ (action)).photos,
      });
    default:
      return state;
  }
}

const root = combineReducers({
  backdrop: backdropReducer,
  loading: loadingReducer,
  local: localReducer,
  currentSelected: currentSelectedReducer,
  pendingSelected: pendingSelectedReducer,
  dailyRefresh: dailyRefreshReducer,
  error: errorReducer,
  fullscreen: fullscreenReducer,
  googlePhotos: googlePhotosReducer,
});

/**
 * Root level reducer for personalization app.
 * @param {!PersonalizationState} state
 * @param {!Action} action
 * @return {!PersonalizationState}
 */
export function reduce(state, action) {
  return root(state, action);
}
