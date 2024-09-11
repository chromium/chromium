// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {AmbientModeAlbum, AmbientTheme, AmbientUiVisibility, TemperatureUnit, TopicSource} from '../../personalization_app.mojom-webui.js';

/**
 * Stores ambient related states.
 */
export interface AmbientState {
  albums: AmbientModeAlbum[]|null;
  ambientModeEnabled: boolean|null;
  ambientTheme: AmbientTheme|null;
  duration: number|
      null;  // number of minutes to run screen saver. 0 means forever.
  previews: Url[]|null;
  temperatureUnit: TemperatureUnit|null;
  topicSource: TopicSource|null;
  ambientUiVisibility: AmbientUiVisibility|null;
  shouldShowTimeOfDayBanner: boolean;
  geolocationPermissionEnabled: boolean|null;
  geolocationIsUserModifiable: boolean|null;
}

export function emptyState(): AmbientState {
  return {
    albums: null,
    ambientModeEnabled: null,
    ambientTheme: null,
    duration: null,
    previews: null,
    temperatureUnit: null,
    topicSource: null,
    ambientUiVisibility: null,
    shouldShowTimeOfDayBanner: false,
    geolocationPermissionEnabled: null,
    geolocationIsUserModifiable: null,
  };
}
