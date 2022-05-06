// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/cr/ui/store.js';

import {BacklightColor} from '../personalization_app.mojom-webui.js';

/**
 * @fileoverview Defines the actions to update keyboard backlight settings.
 */

export enum KeyboardBacklightActionName {
  SET_BACKLIGHT_COLOR = 'set_backlight_color',
}

export type KeyboardBacklightActions = SetBacklightColorAction;

export type SetBacklightColorAction = Action&{
  name: KeyboardBacklightActionName.SET_BACKLIGHT_COLOR,
  backlightColor: BacklightColor,
};

/**
 * Sets the current value of the backlight color.
 */
export function setBacklightColorAction(backlightColor: BacklightColor):
    SetBacklightColorAction {
  return {
    name: KeyboardBacklightActionName.SET_BACKLIGHT_COLOR,
    backlightColor
  };
}
