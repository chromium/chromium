// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/ash/common/store/store.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {ColorScheme} from '../personalization_app.mojom-webui.js';

/**
 * @fileoverview Defines the actions to change theme state.
 */

export enum ThemeActionName {
  SET_DARK_MODE_ENABLED = 'set_dark_mode_enabled',
  SET_COLOR_MODE_AUTO_SCHEDULE_ENABLED = 'set_color_mode_auto_schedule_enabled',
  SET_COLOR_SCHEME = 'set_color_scheme',
  SET_STATIC_COLOR = 'set_static_color',
}

export type ThemeActions = SetColorModeAutoScheduleAction|
    SetDarkModeEnabledAction|SetColorSchemePrefAction|SetStaticColorPrefAction;

export type SetDarkModeEnabledAction = Action&{
  name: ThemeActionName.SET_DARK_MODE_ENABLED,
  enabled: boolean,
};

export type SetColorModeAutoScheduleAction = Action&{
  name: ThemeActionName.SET_COLOR_MODE_AUTO_SCHEDULE_ENABLED,
  enabled: boolean,
};

export type SetColorSchemePrefAction = Action&{
  name: ThemeActionName.SET_COLOR_SCHEME,
  colorScheme: ColorScheme,
};

export type SetStaticColorPrefAction = Action&{
  name: ThemeActionName.SET_STATIC_COLOR,
  staticColor: SkColor | null,
};

export function setDarkModeEnabledAction(enabled: boolean):
    SetDarkModeEnabledAction {
  return {name: ThemeActionName.SET_DARK_MODE_ENABLED, enabled};
}

export function setColorModeAutoScheduleEnabledAction(enabled: boolean):
    SetColorModeAutoScheduleAction {
  return {name: ThemeActionName.SET_COLOR_MODE_AUTO_SCHEDULE_ENABLED, enabled};
}

export function setColorSchemeAction(colorScheme: ColorScheme):
    SetColorSchemePrefAction {
  return {name: ThemeActionName.SET_COLOR_SCHEME, colorScheme};
}

export function setStaticColorAction(staticColor: SkColor|
                                     null): SetStaticColorPrefAction {
  return {name: ThemeActionName.SET_STATIC_COLOR, staticColor};
}
