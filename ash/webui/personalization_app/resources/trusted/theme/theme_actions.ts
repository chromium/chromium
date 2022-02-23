// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/cr/ui/store.js';

/**
 * @fileoverview Defines the actions to change theme state.
 */

export enum ThemeActionName {
  SET_DARK_MODE_ENABLED = 'set_dark_mode_enabled',
}

export type ThemeActions = SetDarkModeEnabledAction;

export type SetDarkModeEnabledAction = Action&{
  name: ThemeActionName.SET_DARK_MODE_ENABLED;
  enabled: boolean;
};

export function setDarkModeEnabledAction(enabled: boolean):
    SetDarkModeEnabledAction {
  return {name: ThemeActionName.SET_DARK_MODE_ENABLED, enabled};
}
