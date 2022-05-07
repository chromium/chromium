// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

/**
 * Specifies page visibility based on incognito status and Chrome OS guest mode.
 */
export type PageVisibility = {
  a11y?: boolean,
  advancedSettings?: boolean,
  appearance?: boolean|AppearancePageVisibility,
  autofill?: boolean,
  defaultBrowser?: boolean,
  downloads?: boolean,
  extensions?: boolean,
  languages?: boolean,
  onStartup?: boolean,
  people?: boolean,
  privacy?: boolean|PrivacyPageVisibility,
  reset?: boolean,
  safetyCheck?: boolean,
  system?: boolean,
};

export type AppearancePageVisibility = {
  bookmarksBar: boolean,
  homeButton: boolean,
  pageZoom: boolean,
  setTheme: boolean,
};

export type PrivacyPageVisibility = {
  networkPrediction: boolean,
  searchPrediction: boolean,
};

/**
 * Dictionary defining page visibility.
 */
export let pageVisibility: PageVisibility;

if (loadTimeData.getBoolean('isGuest')) {
  // "if not chromeos" and "if chromeos" in two completely separate blocks
  // to work around closure compiler.
  // <if expr="not (chromeos_ash or chromeos_lacros)">
  pageVisibility = {
    a11y: false,
    advancedSettings: false,
    appearance: false,
    autofill: false,
    defaultBrowser: false,
    downloads: false,
    extensions: false,
    languages: false,
    onStartup: false,
    people: false,
    privacy: false,
    reset: false,
    safetyCheck: false,
    system: false,
  };
  // </if>
  // <if expr="chromeos_ash or chromeos_lacros">
  pageVisibility = {
    autofill: false,
    people: false,
    onStartup: false,
    reset: false,
    safetyCheck: false,
    appearance: {
      setTheme: false,
      homeButton: false,
      bookmarksBar: false,
      pageZoom: false,
    },
    advancedSettings: true,
    privacy: {
      searchPrediction: false,
      networkPrediction: false,
    },
    downloads: true,
    a11y: true,
    extensions: false,
    languages: true,
  };
  // </if>
}

export function setPageVisibilityForTesting(testVisibility: PageVisibility) {
  pageVisibility = testVisibility;
}
