// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/ash/common/store/store.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {AmbientModeAlbum, AmbientUiVisibility, AnimationTheme, TemperatureUnit, TopicSource} from '../personalization_app.mojom-webui.js';

/**
 * @fileoverview Defines the actions to change ambient state.
 */

export enum AmbientActionName {
  SET_ALBUMS = 'set_albums',
  SET_ALBUM_SELECTED = 'set_album_selected',
  SET_AMBIENT_MODE_ENABLED = 'set_ambient_mode_enabled',
  SET_ANIMATION_THEME = 'set_animation_theme',
  SET_GOOGLE_PHOTOS_ALBUMS_PREVIEWS = 'set_google_photos_albums_previews',
  SET_TEMPERATURE_UNIT = 'set_temperature_unit',
  SET_TOPIC_SOURCE = 'set_topic_source',
  SET_AMBIENT_UI_VISIBILITY = 'set_ambient_ui_visibility',
}

export type AmbientActions =
    SetAlbumsAction|SetAlbumSelectedAction|SetAmbientModeEnabledAction|
    SetAnimationThemeAction|SetGooglePhotosAlbumsPreviewsAction|
    SetTopicSourceAction|SetTemperatureUnitAction|SetAmbientUiVisibilityAction;

export type SetAlbumsAction = Action&{
  name: AmbientActionName.SET_ALBUMS,
  albums: AmbientModeAlbum[],
};

export type SetAlbumSelectedAction = Action&{
  name: AmbientActionName.SET_ALBUM_SELECTED,
};

export type SetAmbientModeEnabledAction = Action&{
  name: AmbientActionName.SET_AMBIENT_MODE_ENABLED,
  enabled: boolean,
};

export type SetAnimationThemeAction = Action&{
  name: AmbientActionName.SET_ANIMATION_THEME,
  animationTheme: AnimationTheme,
};

export type SetGooglePhotosAlbumsPreviewsAction = Action&{
  name: AmbientActionName.SET_GOOGLE_PHOTOS_ALBUMS_PREVIEWS,
  previews: Url[],
};

export type SetTemperatureUnitAction = Action&{
  name: AmbientActionName.SET_TEMPERATURE_UNIT,
  temperatureUnit: TemperatureUnit,
};

export type SetTopicSourceAction = Action&{
  name: AmbientActionName.SET_TOPIC_SOURCE,
  topicSource: TopicSource,
};

export type SetAmbientUiVisibilityAction = Action&{
  name: AmbientActionName.SET_AMBIENT_UI_VISIBILITY,
  ambientUiVisibility: AmbientUiVisibility,
};

/**
 * Sets the current value of the albums.
 */
export function setAlbumsAction(albums: AmbientModeAlbum[]): SetAlbumsAction {
  return {name: AmbientActionName.SET_ALBUMS, albums};
}

export function setAlbumSelectedAction(): SetAlbumSelectedAction {
  return {name: AmbientActionName.SET_ALBUM_SELECTED};
}

/**
 * Sets the current value of the ambient mode pref.
 */
export function setAmbientModeEnabledAction(enabled: boolean):
    SetAmbientModeEnabledAction {
  return {name: AmbientActionName.SET_AMBIENT_MODE_ENABLED, enabled};
}

/**
 * Sets the current value of the animation theme.
 */
export function setAnimationThemeAction(animationTheme: AnimationTheme):
    SetAnimationThemeAction {
  return {name: AmbientActionName.SET_ANIMATION_THEME, animationTheme};
}

/**
 * Sets the current value of Google Photos albums previews.
 */
export function setGooglePhotosAlbumsPreviewsAction(previews: Url[]):
    SetGooglePhotosAlbumsPreviewsAction {
  return {name: AmbientActionName.SET_GOOGLE_PHOTOS_ALBUMS_PREVIEWS, previews};
}

/**
 * Sets the current value of the topic source.
 */
export function setTopicSourceAction(topicSource: TopicSource):
    SetTopicSourceAction {
  return {name: AmbientActionName.SET_TOPIC_SOURCE, topicSource};
}

/**
 * Sets the current value of the temperature unit.
 */
export function setTemperatureUnitAction(temperatureUnit: TemperatureUnit):
    SetTemperatureUnitAction {
  return {name: AmbientActionName.SET_TEMPERATURE_UNIT, temperatureUnit};
}

/**
 * Sets the current state of Ambient UI visibility.
 */
export function setAmbientUiVisibilityAction(
    ambientUiVisibility: AmbientUiVisibility): SetAmbientUiVisibilityAction {
  return {
    name: AmbientActionName.SET_AMBIENT_UI_VISIBILITY,
    ambientUiVisibility,
  };
}
