// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/store.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {AmbientModeAlbum, AmbientTheme, AmbientUiVisibility, TemperatureUnit, TopicSource} from '../../personalization_app.mojom-webui.js';

/**
 * @fileoverview Defines the actions to change ambient state.
 */

export enum AmbientActionName {
  SET_ALBUMS = 'set_albums',
  SET_ALBUM_SELECTED = 'set_album_selected',
  SET_AMBIENT_MODE_ENABLED = 'set_ambient_mode_enabled',
  SET_AMBIENT_THEME = 'set_ambient_theme',
  SET_PREVIEWS = 'set_previews',
  SET_SCREEN_SAVER_DURATION = 'set_screen_saver_duration',
  SET_TEMPERATURE_UNIT = 'set_temperature_unit',
  SET_TOPIC_SOURCE = 'set_topic_source',
  SET_AMBIENT_UI_VISIBILITY = 'set_ambient_ui_visibility',
  SET_SHOULD_SHOW_TIME_OF_DAY_BANNER = 'set_should_show_time_of_day_banner',
  SET_GEOLOCATION_PERMISSION_ENABLED = 'set_geolocation_permission_enabled',
  SET_GEOLOCATION_IS_USER_MODIFIABLE = 'set_geolocation_is_user_modifiable',
}

export type AmbientActions = SetAlbumsAction|SetAlbumSelectedAction|
    SetAmbientModeEnabledAction|SetAmbientThemeAction|SetPreviewsAction|
    SetScreenSaverDurationAction|SetTopicSourceAction|SetTemperatureUnitAction|
    SetAmbientUiVisibilityAction|SetShouldShowTimeOfDayBannerAction|
    SetGeolocationPermissionEnabledAction|SetGeolocationIsUserModifiableAction;

export interface SetAlbumsAction extends Action {
  name: AmbientActionName.SET_ALBUMS;
  albums: AmbientModeAlbum[];
}


export interface SetAlbumSelectedAction extends Action {
  name: AmbientActionName.SET_ALBUM_SELECTED;
}


export interface SetAmbientModeEnabledAction extends Action {
  name: AmbientActionName.SET_AMBIENT_MODE_ENABLED;
  enabled: boolean;
}


export interface SetAmbientThemeAction extends Action {
  name: AmbientActionName.SET_AMBIENT_THEME;
  ambientTheme: AmbientTheme;
}


export interface SetPreviewsAction extends Action {
  name: AmbientActionName.SET_PREVIEWS;
  previews: Url[];
}


export interface SetScreenSaverDurationAction extends Action {
  name: AmbientActionName.SET_SCREEN_SAVER_DURATION;
  minutes: number;
}


export interface SetTemperatureUnitAction extends Action {
  name: AmbientActionName.SET_TEMPERATURE_UNIT;
  temperatureUnit: TemperatureUnit;
}


export interface SetTopicSourceAction extends Action {
  name: AmbientActionName.SET_TOPIC_SOURCE;
  topicSource: TopicSource;
}


export interface SetAmbientUiVisibilityAction extends Action {
  name: AmbientActionName.SET_AMBIENT_UI_VISIBILITY;
  ambientUiVisibility: AmbientUiVisibility;
}


export interface SetShouldShowTimeOfDayBannerAction extends Action {
  name: AmbientActionName.SET_SHOULD_SHOW_TIME_OF_DAY_BANNER;
  shouldShowTimeOfDayBanner: boolean;
}

export interface SetGeolocationPermissionEnabledAction extends Action {
  name: AmbientActionName.SET_GEOLOCATION_PERMISSION_ENABLED;
  enabled: boolean;
}

export interface SetGeolocationIsUserModifiableAction extends Action {
  name: AmbientActionName.SET_GEOLOCATION_IS_USER_MODIFIABLE;
  isUserModifiable: boolean;
}


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
 * Sets the current value of the ambient theme.
 */
export function setAmbientThemeAction(ambientTheme: AmbientTheme):
    SetAmbientThemeAction {
  return {name: AmbientActionName.SET_AMBIENT_THEME, ambientTheme};
}

/**
 * Sets the current value of preview image URLs.
 */
export function setPreviewsAction(previews: Url[]): SetPreviewsAction {
  return {name: AmbientActionName.SET_PREVIEWS, previews};
}

/**
 * Sets the current value of the screen saver duration.
 */
export function setScreenSaverDurationAction(minutes: number):
    SetScreenSaverDurationAction {
  return {name: AmbientActionName.SET_SCREEN_SAVER_DURATION, minutes};
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

/**
 * Sets the boolean that determines whether to show the time of day banner.
 */
export function setShouldShowTimeOfDayBannerAction(
    shouldShowTimeOfDayBanner: boolean): SetShouldShowTimeOfDayBannerAction {
  return {
    name: AmbientActionName.SET_SHOULD_SHOW_TIME_OF_DAY_BANNER,
    shouldShowTimeOfDayBanner,
  };
}

export function setGeolocationPermissionEnabledAction(enabled: boolean):
    SetGeolocationPermissionEnabledAction {
  return {name: AmbientActionName.SET_GEOLOCATION_PERMISSION_ENABLED, enabled};
}

export function setGeolocationIsUserModifiableAction(isUserModifiable: boolean):
    SetGeolocationIsUserModifiableAction {
  return {
    name: AmbientActionName.SET_GEOLOCATION_IS_USER_MODIFIABLE,
    isUserModifiable,
  };
}
