// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/cr/ui/store.js';

/**
 * @fileoverview Defines the actions to change ambient state.
 */

export enum AmbientActionName {
  SET_AMBIENT_MODE_ENABLED = 'set_ambient_mode_enabled',
}

export type AmbientActions = SetAmbientModeEnabledAction;

export type SetAmbientModeEnabledAction = Action&{
  name: AmbientActionName.SET_AMBIENT_MODE_ENABLED;
  enabled: boolean;
};

/**
 * Sets the current value of the ambient mode pref.
 */
export function setAmbientModeEnabledAction(enabled: boolean):
    SetAmbientModeEnabledAction {
  return {name: AmbientActionName.SET_AMBIENT_MODE_ENABLED, enabled};
}
