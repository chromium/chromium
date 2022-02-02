// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AmbientProviderInterface} from '../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';
import {setAmbientModeEnabledAction} from './ambient_actions.js';

/**
 * @fileoverview contains all of the functions to interact with ambient mode
 * related C++ side through mojom calls. Handles setting |PersonalizationStore|
 * state in response to mojom data.
 */

// Enable or disable ambient mode.
export function setAmbientModeEnabled(
    ambientModeEnabled: boolean, provider: AmbientProviderInterface,
    store: PersonalizationStore): void {
  provider.setAmbientModeEnabled(ambientModeEnabled);

  // Dispatch action to toggle the button to indicate if the ambient mode is
  // enabled.
  store.dispatch(setAmbientModeEnabledAction(ambientModeEnabled));
}
