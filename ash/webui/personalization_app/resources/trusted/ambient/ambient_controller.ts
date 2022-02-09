// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AmbientProviderInterface, TopicSource} from '../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';
import {setAmbientModeEnabledAction, setTopicSourceAction} from './ambient_actions.js';

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

// Set ambient mode topic source.
export function setTopicSource(
    topicSource: TopicSource, provider: AmbientProviderInterface,
    store: PersonalizationStore): void {
  provider.setTopicSource(topicSource);

  // Dispatch action to select topic source.
  store.dispatch(setTopicSourceAction(topicSource));
}
