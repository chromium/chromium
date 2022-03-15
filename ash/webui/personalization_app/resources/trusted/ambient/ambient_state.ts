// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AmbientModeAlbum, AnimationTheme, TemperatureUnit, TopicSource} from '../personalization_app.mojom-webui.js';

/**
 * Stores ambient related states.
 */
export interface AmbientState {
  albums: AmbientModeAlbum[]|null;
  ambientModeEnabled: boolean|null;
  animationTheme: AnimationTheme|null;
  temperatureUnit: TemperatureUnit|null;
  topicSource: TopicSource|null;
}

export function emptyState(): AmbientState {
  return {
    albums: null,
    ambientModeEnabled: null,
    animationTheme: null,
    temperatureUnit: null,
    topicSource: null,
  };
}
