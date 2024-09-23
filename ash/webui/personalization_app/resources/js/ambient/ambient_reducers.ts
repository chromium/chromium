// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Actions} from '../personalization_actions.js';
import {ReducerFunction} from '../personalization_reducers.js';
import {PersonalizationState} from '../personalization_state.js';

import {AmbientActionName} from './ambient_actions.js';
import {AmbientState} from './ambient_state.js';

export function albumsReducer(
    state: AmbientState['albums'], action: Actions,
    _: PersonalizationState): AmbientState['albums'] {
  switch (action.name) {
    case AmbientActionName.SET_ALBUMS:
      return action.albums;
    case AmbientActionName.SET_ALBUM_SELECTED:
      if (!state) {
        return state;
      }
      // An albums in AmbientState.albums is mutated by setting checked
      // to True/False, have to return a copy of albums state so that
      // Polymer knows there is an update.
      return [...state];
    default:
      return state;
  }
}

export function ambientModeEnabledReducer(
    state: AmbientState['ambientModeEnabled'], action: Actions,
    _: PersonalizationState): AmbientState['ambientModeEnabled'] {
  switch (action.name) {
    case AmbientActionName.SET_AMBIENT_MODE_ENABLED:
      return action.enabled;
    default:
      return state;
  }
}

export function ambientThemeReducer(
    state: AmbientState['ambientTheme'], action: Actions,
    _: PersonalizationState): AmbientState['ambientTheme'] {
  switch (action.name) {
    case AmbientActionName.SET_AMBIENT_THEME:
      return action.ambientTheme;
    default:
      return state;
  }
}

export function previewsReducer(
    state: AmbientState['previews'], action: Actions,
    _: PersonalizationState): AmbientState['previews'] {
  switch (action.name) {
    case AmbientActionName.SET_PREVIEWS:
      return action.previews;
    default:
      return state;
  }
}

export function screenSaverDurationReducer(
    state: number|null, action: Actions, _: PersonalizationState): number|null {
  switch (action.name) {
    case AmbientActionName.SET_SCREEN_SAVER_DURATION:
      return action.minutes;
    default:
      return state;
  }
}

export function temperatureUnitReducer(
    state: AmbientState['temperatureUnit'], action: Actions,
    _: PersonalizationState): AmbientState['temperatureUnit'] {
  switch (action.name) {
    case AmbientActionName.SET_TEMPERATURE_UNIT:
      return action.temperatureUnit;
    default:
      return state;
  }
}

export function topicSourceReducer(
    state: AmbientState['topicSource'], action: Actions,
    _: PersonalizationState): AmbientState['topicSource'] {
  switch (action.name) {
    case AmbientActionName.SET_TOPIC_SOURCE:
      return action.topicSource;
    default:
      return state;
  }
}

export function ambientUiVisibilityReducer(
    state: AmbientState['ambientUiVisibility'], action: Actions,
    _: PersonalizationState): AmbientState['ambientUiVisibility'] {
  switch (action.name) {
    case AmbientActionName.SET_AMBIENT_UI_VISIBILITY:
      return action.ambientUiVisibility;
    default:
      return state;
  }
}

export function shouldShowTimeOfDayBannerReducer(
    state: boolean, action: Actions, _: PersonalizationState): boolean {
  switch (action.name) {
    case AmbientActionName.SET_SHOULD_SHOW_TIME_OF_DAY_BANNER:
      return action.shouldShowTimeOfDayBanner;
    default:
      return state;
  }
}

export function geolocationPermissionEnabledReducer(
    state: AmbientState['geolocationPermissionEnabled'], action: Actions,
    _: PersonalizationState): AmbientState['geolocationPermissionEnabled'] {
  switch (action.name) {
    case AmbientActionName.SET_GEOLOCATION_PERMISSION_ENABLED:
      return action.enabled;
    default:
      return state;
  }
}

export function geolocationIsUserModifiableReducer(
    state: AmbientState['geolocationIsUserModifiable'], action: Actions,
    _: PersonalizationState): AmbientState['geolocationIsUserModifiable'] {
  switch (action.name) {
    case AmbientActionName.SET_GEOLOCATION_IS_USER_MODIFIABLE:
      return action.isUserModifiable;
    default:
      return state;
  }
}


export const ambientReducers:
    {[K in keyof AmbientState]: ReducerFunction<AmbientState[K]>} = {
      albums: albumsReducer,
      ambientModeEnabled: ambientModeEnabledReducer,
      ambientTheme: ambientThemeReducer,
      duration: screenSaverDurationReducer,
      previews: previewsReducer,
      temperatureUnit: temperatureUnitReducer,
      topicSource: topicSourceReducer,
      ambientUiVisibility: ambientUiVisibilityReducer,
      shouldShowTimeOfDayBanner: shouldShowTimeOfDayBannerReducer,
      geolocationPermissionEnabled: geolocationPermissionEnabledReducer,
      geolocationIsUserModifiable: geolocationIsUserModifiableReducer,
    };
