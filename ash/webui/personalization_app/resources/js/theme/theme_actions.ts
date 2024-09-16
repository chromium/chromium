// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/store.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {ColorScheme} from '../../color_scheme.mojom-webui.js';
import {SampleColorScheme} from '../../personalization_app.mojom-webui.js';

/**
 * @fileoverview Defines the actions to change theme state.
 */

export enum ThemeActionName {
  SET_DARK_MODE_ENABLED = 'set_dark_mode_enabled',
  SET_COLOR_MODE_AUTO_SCHEDULE_ENABLED = 'set_color_mode_auto_schedule_enabled',
  SET_COLOR_SCHEME = 'set_color_scheme',
  SET_SAMPLE_COLOR_SCHEMES = 'set_sample_color_schemes',
  SET_STATIC_COLOR = 'set_static_color',
  SET_GEOLOCATION_PERMISSION_ENABLED = 'set_geolocation_permission_enabled',
  SET_SUNRISE_TIME = 'set_sunrise_time',
  SET_SUNSET_TIME = 'set_sunset_time',
  SET_GEOLOCATION_IS_USER_MODIFIABLE = 'set_geolocation_is_user_modifiable',
}

export type ThemeActions =
    SetColorModeAutoScheduleAction|SetDarkModeEnabledAction|
    SetColorSchemeAction|SetSampleColorSchemesAction|SetStaticColorAction|
    SetGeolocationPermissionEnabledAction|SetSunriseTimeAction|
    SetSunsetTimeAction|SetGeolocationIsUserModifiableAction;

export interface SetDarkModeEnabledAction extends Action {
  name: ThemeActionName.SET_DARK_MODE_ENABLED;
  enabled: boolean;
}


export interface SetColorModeAutoScheduleAction extends Action {
  name: ThemeActionName.SET_COLOR_MODE_AUTO_SCHEDULE_ENABLED;
  enabled: boolean;
}


export interface SetColorSchemeAction extends Action {
  name: ThemeActionName.SET_COLOR_SCHEME;
  colorScheme: ColorScheme;
}


export interface SetSampleColorSchemesAction extends Action {
  name: ThemeActionName.SET_SAMPLE_COLOR_SCHEMES;
  sampleColorSchemes: SampleColorScheme[];
}


export interface SetStaticColorAction extends Action {
  name: ThemeActionName.SET_STATIC_COLOR;
  staticColor: SkColor|null;
}

export interface SetGeolocationPermissionEnabledAction extends Action {
  name: ThemeActionName.SET_GEOLOCATION_PERMISSION_ENABLED;
  enabled: boolean;
}

export interface SetGeolocationIsUserModifiableAction extends Action {
  name: ThemeActionName.SET_GEOLOCATION_IS_USER_MODIFIABLE;
  isUserModifiable: boolean;
}

export interface SetSunriseTimeAction extends Action {
  name: ThemeActionName.SET_SUNRISE_TIME;
  time: string;
}

export interface SetSunsetTimeAction extends Action {
  name: ThemeActionName.SET_SUNSET_TIME;
  time: string;
}

export function setDarkModeEnabledAction(enabled: boolean):
    SetDarkModeEnabledAction {
  return {name: ThemeActionName.SET_DARK_MODE_ENABLED, enabled};
}

export function setColorModeAutoScheduleEnabledAction(enabled: boolean):
    SetColorModeAutoScheduleAction {
  return {name: ThemeActionName.SET_COLOR_MODE_AUTO_SCHEDULE_ENABLED, enabled};
}

export function setColorSchemeAction(colorScheme: ColorScheme):
    SetColorSchemeAction {
  return {name: ThemeActionName.SET_COLOR_SCHEME, colorScheme};
}

export function setSampleColorSchemesAction(
    sampleColorSchemes: SampleColorScheme[]): SetSampleColorSchemesAction {
  return {name: ThemeActionName.SET_SAMPLE_COLOR_SCHEMES, sampleColorSchemes};
}

export function setStaticColorAction(staticColor: SkColor|
                                     null): SetStaticColorAction {
  return {name: ThemeActionName.SET_STATIC_COLOR, staticColor};
}

export function setGeolocationPermissionEnabledAction(enabled: boolean):
    SetGeolocationPermissionEnabledAction {
  return {name: ThemeActionName.SET_GEOLOCATION_PERMISSION_ENABLED, enabled};
}

export function setGeolocationIsUserModifiableAction(isUserModifiable: boolean):
    SetGeolocationIsUserModifiableAction {
  return {
    name: ThemeActionName.SET_GEOLOCATION_IS_USER_MODIFIABLE,
    isUserModifiable,
  };
}

export function setSunriseTimeAction(time: string): SetSunriseTimeAction {
  return {name: ThemeActionName.SET_SUNRISE_TIME, time};
}

export function setSunsetTimeAction(time: string): SetSunsetTimeAction {
  return {name: ThemeActionName.SET_SUNSET_TIME, time};
}
