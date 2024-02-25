// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview  Defines reducers for personalization app.  Reducers must be a
 * pure function that returns a new state object if anything has changed.
 * @see [redux tutorial]{@link https://redux.js.org/tutorials/fundamentals/part-3-state-actions-reducers}
 */
import {isNonEmptyArray} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {ambientReducers} from './ambient/ambient_reducers.js';
import {AmbientState} from './ambient/ambient_state.js';
import {keyboardBacklightReducers} from './keyboard_backlight/keyboard_backlight_reducers.js';
import {KeyboardBacklightState} from './keyboard_backlight/keyboard_backlight_state.js';
import {Actions, PersonalizationActionName} from './personalization_actions.js';
import {PersonalizationState} from './personalization_state.js';
import {themeReducers} from './theme/theme_reducers.js';
import {ThemeState} from './theme/theme_state.js';
import {userReducers} from './user/user_reducers.js';
import {UserState} from './user/user_state.js';
import {WallpaperActionName} from './wallpaper/wallpaper_actions.js';
import {wallpaperReducers} from './wallpaper/wallpaper_reducers.js';
import {WallpaperState} from './wallpaper/wallpaper_state.js';

export type ReducerFunction<State> =
    (state: State, action: Actions, globalState: PersonalizationState) => State;

/**
 * Combines reducers into a single top level reducer. Inspired by Redux's
 * |combineReducers| functions.
 */
function combineReducers<T extends {}>(
    mapping: {[K in keyof T]: ReducerFunction<T[K]>}):
    (state: T, action: Actions, globalState: PersonalizationState) => T {
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
    _: PersonalizationState): PersonalizationState['error'] {
  switch (action.name) {
    case WallpaperActionName.END_SELECT_IMAGE:
      const {success} = action;
      if (success) {
        return null;
      }
      return {message: loadTimeData.getString('setWallpaperError')};
    case WallpaperActionName.SET_SELECTED_IMAGE:
      const {image} = action;
      if (image) {
        return state;
      }
      return {message: loadTimeData.getString('loadWallpaperError')};
    // Show network error toast if local images are available but online
    // collections are failed to load. As local images include at least
    // the default image, we only need to check the status of online
    // collections.
    case WallpaperActionName.SET_COLLECTIONS:
      const {collections} = action;
      if (!isNonEmptyArray(collections)) {
        return {message: loadTimeData.getString('wallpaperNetworkError')};
      }
      return state;
    case PersonalizationActionName.SET_ERROR:
      if (state && state.dismiss && state.dismiss.callback) {
        state.dismiss.callback(/*fromUser=*/ false);
      }
      return action.error;
    case PersonalizationActionName.DISMISS_ERROR:
      if (!state) {
        console.warn(
            'Received dismiss error action when error is already null');
        return null;
      }
      if (action.id && (!state.id || action.id !== state.id)) {
        return state;
      }
      if (state && state.dismiss && state.dismiss.callback) {
        state.dismiss.callback(action.fromUser);
      }
      return null;
    default:
      return state;
  }
}

const root = combineReducers<PersonalizationState>({
  error: errorReducer,
  ambient: combineReducers<AmbientState>(ambientReducers),
  keyboardBacklight:
      combineReducers<KeyboardBacklightState>(keyboardBacklightReducers),
  theme: combineReducers<ThemeState>(themeReducers),
  user: combineReducers<UserState>(userReducers),
  wallpaper: combineReducers<WallpaperState>(wallpaperReducers),
});

export function reduce(
    state: PersonalizationState, action: Actions): PersonalizationState {
  return root(state, action, state);
}
