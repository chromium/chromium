// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {AmbientModeAlbum, AmbientUiVisibility, AnimationTheme, TemperatureUnit, TopicSource} from '../../personalization_app.mojom-webui.js';

/**
 * Stores ambient related states.
 */
export interface AmbientState {
  albums: AmbientModeAlbum[]|null;
  ambientModeEnabled: boolean|null;
  animationTheme: AnimationTheme|null;
  duration: number|
      null;  // number of minutes to run screen saver. 0 means forever.
  previews: Url[]|null;
  temperatureUnit: TemperatureUnit|null;
  topicSource: TopicSource|null;
  ambientUiVisibility: AmbientUiVisibility|null;
  shouldShowTimeOfDayBanner: boolean;
}

export function emptyState(): AmbientState {
  return {
    albums: null,
    ambientModeEnabled: null,
    animationTheme: null,
    duration: null,
    previews: null,
    temperatureUnit: null,
    topicSource: null,
    ambientUiVisibility: null,
    shouldShowTimeOfDayBanner: false,
  };
}
