// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BacklightColor, KeyboardBacklightProviderInterface} from '../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';

import {setBacklightColorAction} from './keyboard_backlight_actions.js';

/**
 * @fileoverview contains all of the functions to interact with keyboard
 * backlight related C++ side through mojom calls. Handles setting
 * |PersonalizationStore| state in response to mojom data.
 */

// Set the keyboard backlight color.
export function setBacklightColor(
    backlightColor: BacklightColor,
    provider: KeyboardBacklightProviderInterface, store: PersonalizationStore) {
  provider.setBacklightColor(backlightColor);

  // Dispatch action to highlight backlight color.
  store.dispatch(setBacklightColorAction(backlightColor));
}
