// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview  Defines reducers for personalization app.  Reducers must be a
 * pure function that returns a new state object if anything has changed.
 * @see [redux tutorial]{@link https://redux.js.org/tutorials/fundamentals/part-3-state-actions-reducers}
 */

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {ActionName} from './personalization_actions.js';
import {Actions} from './personalization_actions.js';
import {CurrentWallpaper, WallpaperCollection, WallpaperImage} from './personalization_app.mojom-webui.js';
import {BackdropState, DailyRefreshState, GooglePhotosState, LoadingState, LocalState, PersonalizationState, States} from './personalization_state.js';

export type DisplayableImage = FilePath|WallpaperImage;

export type ReducerFunction<State extends States> =
    (state: State, action: Actions, globalState: PersonalizationState) => State;

export interface PersonalizationReducer {
  backdrop: ReducerFunction<BackdropState>;
  loading: ReducerFunction<LoadingState>;
  local: ReducerFunction<LocalState>;
  currentSelected: ReducerFunction<CurrentWallpaper|null>;
  pendingSelected: ReducerFunction<WallpaperImage|FilePath|null>;
  dailyRefresh: ReducerFunction<DailyRefreshState>;
  error: ReducerFunction<string|null>;
  fullscreen: ReducerFunction<boolean>;
  googlePhotos: ReducerFunction<GooglePhotosState>;
}

/**
 * Combines reducers into a single top level reducer. Inspired by Redux's
 * |combineReducers| functions.
 */
function combineReducers(mapping: PersonalizationReducer): (
    state: PersonalizationState, action: Actions) => PersonalizationState {
  function reduce(
      state: PersonalizationState, action: Actions): PersonalizationState {
    const newState =
        (Object.keys(mapping) as Array<keyof PersonalizationReducer>)
            .reduce((result, key) => {
              const func = mapping[key] as ReducerFunction<States>;
              // The type here is too dynamic to spec out.
              (result as any)[key] = func(state[key], action, state);
              return result;
            }, {} as PersonalizationState);
    const change = (Object.keys(state) as Array<keyof PersonalizationState>)
                       .some((key) => newState[key] !== state[key]);
    return change ? newState : state;
  }
  return reduce;
}

function backdropReducer(
    state: BackdropState, action: Actions,
    _: PersonalizationState): BackdropState {
  switch (action.name) {
    case ActionName.SET_COLLECTIONS:
      return {collections: action.collections, images: {}};
    case ActionName.SET_IMAGES_FOR_COLLECTION:
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
        images: {...state.images, [action.collectionId]: action.images}
      };
    default:
      return state;
  }
}

function loadingReducer(
    state: LoadingState, action: Actions,
    _: PersonalizationState): LoadingState {
  switch (action.name) {
    case ActionName.BEGIN_LOAD_IMAGES_FOR_COLLECTIONS:
      return {
        ...state,
        images: action.collections.reduce(
            (result, {id}) => {
              result[id] = true;
              return result;
            },
            {} as Record<WallpaperCollection['id'], boolean>)
      };
    case ActionName.BEGIN_LOAD_LOCAL_IMAGE_DATA:
      return {
        ...state,
        local: {...state.local, data: {...state.local.data, [action.id]: true}}
      };
    case ActionName.BEGIN_LOAD_SELECTED_IMAGE:
      return {...state, selected: true};
    case ActionName.BEGIN_SELECT_IMAGE:
      return {...state, setImage: state.setImage + 1};
    case ActionName.END_SELECT_IMAGE:
      if (state.setImage <= 0) {
        console.error('Impossible state for loading.setImage');
        // Reset to 0.
        return {...state, setImage: 0};
      }
      return {...state, setImage: state.setImage - 1};
    case ActionName.SET_COLLECTIONS:
      return {...state, collections: false};
    case ActionName.SET_IMAGES_FOR_COLLECTION:
      return {
        ...state,
        images: {...state.images, [action.collectionId]: false},
      };
    case ActionName.BEGIN_LOAD_LOCAL_IMAGES:
      return {
        ...state,
        local: {
          ...state.local,
          images: true,
        },
      };
    case ActionName.SET_LOCAL_IMAGES:
      return {
        ...state,
        local: {
          // Only keep loading state for most recent local images.
          data: (action.images || [])
                    .reduce(
                        (result, {path}) => {
                          if (state.local.data.hasOwnProperty(path)) {
                            result[path] = state.local.data[path];
                          }
                          return result;
                        },
                        {} as Record<FilePath['path'], boolean>),
          // Image list is done loading.
          images: false,
        },
      };
    case ActionName.SET_LOCAL_IMAGE_DATA:
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
    case ActionName.SET_SELECTED_IMAGE:
      if (state.setImage === 0) {
        return {...state, selected: false};
      }
      return state;
    case ActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE:
      return {...state, refreshWallpaper: true};
    case ActionName.SET_UPDATED_DAILY_REFRESH_IMAGE:
      return {...state, refreshWallpaper: false};
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM:
      assert(state.googlePhotos.photosByAlbumId[action.albumId] === undefined);
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
    case ActionName.SET_GOOGLE_PHOTOS_ALBUM:
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
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS:
      assert(state.googlePhotos.albums === false);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          albums: true,
        },
      };
    case ActionName.SET_GOOGLE_PHOTOS_ALBUMS:
      assert(state.googlePhotos.albums === true);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          albums: false,
        },
      };
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_COUNT:
      assert(state.googlePhotos.count === false);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          count: true,
        },
      };
    case ActionName.SET_GOOGLE_PHOTOS_COUNT:
      assert(state.googlePhotos.count === true);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          count: false,
        },
      };
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS:
      assert(state.googlePhotos.photos === false);
      return {
        ...state,
        googlePhotos: {
          ...state.googlePhotos,
          photos: true,
        },
      };
    case ActionName.SET_GOOGLE_PHOTOS_PHOTOS:
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
    state: LocalState, action: Actions, _: PersonalizationState): LocalState {
  switch (action.name) {
    case ActionName.SET_LOCAL_IMAGES:
      return {
        ...state,
        images: action.images,
        // Only keep image thumbnails if the image is still in |images|.
        data: (action.images || [])
                  .reduce(
                      (result, {path}) => {
                        if (state.data.hasOwnProperty(path)) {
                          result[path] = state.data[path];
                        }
                        return result;
                      },
                      {} as Record<FilePath['path'], string>),
      };
    case ActionName.SET_LOCAL_IMAGE_DATA:
      return {
        ...state,
        data: {
          ...state.data,
          [action.id]: action.data,
        }
      };
    default:
      return state;
  }
}

function currentSelectedReducer(
    state: CurrentWallpaper|null, action: Actions,
    _: PersonalizationState): CurrentWallpaper|null {
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
 */
function pendingSelectedReducer(
    state: DisplayableImage|null, action: Actions,
    globalState: PersonalizationState): DisplayableImage|null {
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
      } else if (globalState.loading.setImage == 0) {
        // Clear the pending state when there are no more requests.
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
      const {success} = action;
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

function dailyRefreshReducer(
    state: DailyRefreshState, action: Actions,
    _: PersonalizationState): DailyRefreshState {
  switch (action.name) {
    case ActionName.SET_DAILY_REFRESH_COLLECTION_ID:
      return {
        ...state,
        collectionId: action.collectionId,
      };
    default:
      return state;
  }
}

function errorReducer(
    state: string|null, action: Actions, _: PersonalizationState): string|null {
  switch (action.name) {
    case ActionName.END_SELECT_IMAGE:
      const {success} = action;
      if (success) {
        return null;
      }
      return state || loadTimeData.getString('setWallpaperError');
    case ActionName.SET_SELECTED_IMAGE:
      const {image} = action;
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

function fullscreenReducer(
    state: boolean, action: Actions, _: PersonalizationState): boolean {
  switch (action.name) {
    case ActionName.SET_FULLSCREEN_ENABLED:
      return action.enabled;
    default:
      return state;
  }
}

function googlePhotosReducer(
    state: GooglePhotosState, action: Actions,
    _: PersonalizationState): GooglePhotosState {
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
      return {
        ...state,
        photosByAlbumId: {
          ...state.photosByAlbumId,
          [action.albumId]: action.photos,
        },
      };
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS:
      // The list of albums should be loaded only once.
      assert(state.albums === undefined);
      return state;
    case ActionName.SET_GOOGLE_PHOTOS_ALBUMS:
      assert(action.albums !== undefined);
      return {
        ...state,
        albums: action.albums,
      };
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_COUNT:
      // The total count of photos should be loaded only once.
      assert(state.count === undefined);
      return state;
    case ActionName.SET_GOOGLE_PHOTOS_COUNT:
      assert(action.count !== undefined);
      return {
        ...state,
        count: action.count,
      };
    case ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS:
      // The list of photos should be loaded only once.
      assert(state.photos === undefined);
      return state;
    case ActionName.SET_GOOGLE_PHOTOS_PHOTOS:
      assert(action.photos !== undefined);
      return {
        ...state,
        photos: action.photos,
      };
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

export function reduce(
    state: PersonalizationState, action: Actions): PersonalizationState {
  return root(state, action);
}
