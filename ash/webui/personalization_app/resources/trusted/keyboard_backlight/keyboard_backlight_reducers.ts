// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Actions} from '../personalization_actions.js';
import {BacklightColor} from '../personalization_app.mojom-webui.js';
import {ReducerFunction} from '../personalization_reducers.js';
import {PersonalizationState} from '../personalization_state.js';

import {KeyboardBacklightActionName} from './keyboard_backlight_actions.js';
import {KeyboardBacklightState} from './keyboard_backlight_state.js';

export function backlightColorReducer(
    state: BacklightColor|null, action: Actions,
    _: PersonalizationState): BacklightColor|null {
  switch (action.name) {
    case KeyboardBacklightActionName.SET_BACKLIGHT_COLOR:
      return action.backlightColor;
    default:
      return state;
  }
}

export const keyboardBacklightReducers: {
  [K in keyof KeyboardBacklightState]:
      ReducerFunction<KeyboardBacklightState[K]>
} = {
  backlightColor: backlightColorReducer,
};
