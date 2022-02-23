// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ThemeProviderInterface} from '../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';
import {setDarkModeEnabledAction} from './theme_actions.js';

/**
 * @fileoverview contains all of the functions to interact with C++ side through
 * mojom calls. Handles setting |PersonalizationStore| state in response to
 * mojom data.
 */

// Disables or enables dark color mode.
export async function setColorModePref(
    darkModeEnabled: boolean, provider: ThemeProviderInterface,
    store: PersonalizationStore): Promise<void> {
  await provider.setColorModePref(darkModeEnabled);
  // Dispatch action to highlight color mode.
  store.dispatch(setDarkModeEnabledAction(darkModeEnabled));
}
