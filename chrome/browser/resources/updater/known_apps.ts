// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utilities for accessing the set of apps known to this browser.
 * Dynamic accessors over loadTimeData are used instead of constants to allow
 * the set to be overridden in tests.
 */

import {loadTimeData} from './i18n_setup.js';

/**
 * Produces a map of app names to their lower-case app IDs.
 */
export function getKnownApps(): Map<string, string[]> {
  const numApps = loadTimeData.getInteger('numKnownApps');
  const apps = new Map<string, string[]>();
  for (let i = 0; i < numApps; i++) {
    apps.set(
        loadTimeData.getString(`knownAppName${i}`),
        loadTimeData.getString(`knownAppIds${i}`)
            .split(',')
            .map(appId => appId.toLowerCase()));
  }
  return apps;
}

/**
 * Produces a map of app IDs to app names as the inverse of getKnownApps.
 */
export function getKnownAppNamesById(): Map<string, string> {
  return new Map<string, string>(
      Array.from(getKnownApps().entries())
          .flatMap(
              ([appName, appIds]) => appIds.map(appId => [appId, appName])));
}
