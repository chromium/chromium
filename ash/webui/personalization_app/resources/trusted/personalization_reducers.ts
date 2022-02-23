// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview  Defines reducers for personalization app.  Reducers must be a
 * pure function that returns a new state object if anything has changed.
 * @see [redux tutorial]{@link https://redux.js.org/tutorials/fundamentals/part-3-state-actions-reducers}
 */
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {isNonEmptyArray} from '../common/utils.js';

import {ambientReducers} from './ambient/ambient_reducers.js';
import {AmbientState} from './ambient/ambient_state.js';
import {PersonalizationActionName} from './personalization_actions.js';
import {Actions} from './personalization_actions.js';
import {GooglePhotosPhoto, WallpaperImage} from './personalization_app.mojom-webui.js';
import {PersonalizationState} from './personalization_state.js';
import {themeReducers} from './theme/theme_reducers.js';
import {ThemeState} from './theme/theme_state.js';
import {userReducers} from './user/user_reducers.js';
import {UserState} from './user/user_state.js';
import {WallpaperActionName} from './wallpaper/wallpaper_actions.js';
import {wallpaperReducers} from './wallpaper/wallpaper_reducers.js';
import {WallpaperState} from './wallpaper/wallpaper_state.js';

export type DisplayableImage = FilePath|GooglePhotosPhoto|WallpaperImage;

export type ReducerFunction<State> =
    (state: State, action: Actions, globalState: PersonalizationState) => State;

/**
 * Combines reducers into a single top level reducer. Inspired by Redux's
 * |combineReducers| functions.
 */
function combineReducers<T>(mapping: {[K in keyof T]: ReducerFunction<T[K]>}): (
    state: T, action: Actions, globalState: PersonalizationState) => T {
  function reduce(
      state: T, action: Actions, globalState: PersonalizationState): T {
    const newState: T =
        (Object.keys(mapping) as Array<keyof T>).reduce((result, key) => {
          const func = mapping[key] as ReducerFunction<T[typeof key]>;
          result[key] = func(state[key], action, globalState);
          return result;
        }, {} as T);
    const change = (Object.keys(state) as Array<keyof T>)
                       .some((key) => newState[key] !== state[key]);
    return change ? newState : state;
  }
  return reduce;
}

function errorReducer(
    state: PersonalizationState['error'], action: Actions,
    globalState: PersonalizationState): PersonalizationState['error'] {
  switch (action.name) {
    case WallpaperActionName.END_SELECT_IMAGE:
      const {success} = action;
      if (success) {
        return null;
      }
      return state || loadTimeData.getString('setWallpaperError');
    case WallpaperActionName.SET_SELECTED_IMAGE:
      const {image} = action;
      if (image) {
        return state;
      }
      return state || loadTimeData.getString('loadWallpaperError');
    // Show network error toast if local images are available but online
    // collections are failed to load. As local images and online collections
    // are loaded asynchronously, we need to check the above condition for both
    // SET_LOCAL_IMAGES and SET_COLLECTIONS actions.
    case WallpaperActionName.SET_LOCAL_IMAGES:
      const {images} = action;
      if (isNonEmptyArray(images) &&
          !globalState.wallpaper.loading.collections &&
          !isNonEmptyArray(globalState.wallpaper.backdrop.collections)) {
        return state || loadTimeData.getString('networkError');
      }
      return state;
    case WallpaperActionName.SET_COLLECTIONS:
      const {collections} = action;
      if (!globalState.wallpaper.loading.local.images &&
          isNonEmptyArray(globalState.wallpaper.local.images) &&
          !isNonEmptyArray(collections)) {
        return state || loadTimeData.getString('networkError');
      }
      return state;
    case PersonalizationActionName.DISMISS_ERROR:
      if (!state) {
        console.warn(
            'Received dismiss error action when error is already null');
      }
      return null;
    default:
      return state;
  }
}

const root = combineReducers<PersonalizationState>({
  error: errorReducer,
  ambient: combineReducers<AmbientState>(ambientReducers),
  theme: combineReducers<ThemeState>(themeReducers),
  user: combineReducers<UserState>(userReducers),
  wallpaper: combineReducers<WallpaperState>(wallpaperReducers),
});

export function reduce(
    state: PersonalizationState, action: Actions): PersonalizationState {
  return root(state, action, state);
}
