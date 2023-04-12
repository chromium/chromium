// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

/**
 * Specifies page visibility based on incognito status and Chrome OS guest mode.
 */
export interface PageVisibility {
  a11y?: boolean;
  advancedSettings?: boolean;
  appearance?: boolean|AppearancePageVisibility;
  autofill?: boolean;
  defaultBrowser?: boolean;
  downloads?: boolean;
  extensions?: boolean;
  getMostChrome?: boolean;
  languages?: boolean;
  onStartup?: boolean;
  people?: boolean;
  performance?: boolean;
  privacy?: boolean|PrivacyPageVisibility;
  reset?: boolean;
  safetyCheck?: boolean;
  system?: boolean;
}

export interface AppearancePageVisibility {
  bookmarksBar: boolean;
  homeButton: boolean;
  pageZoom: boolean;
  setTheme: boolean;
  sidePanel: boolean;
}

export interface PrivacyPageVisibility {
  networkPrediction: boolean;
  searchPrediction: boolean;
}

/**
 * Dictionary defining page visibility.
 */
export let pageVisibility: PageVisibility;

if (loadTimeData.getBoolean('isGuest')) {
  // "if not chromeos" and "if chromeos" in two completely separate blocks
  // to work around closure compiler.
  // <if expr="not is_chromeos">
  pageVisibility = {
    a11y: false,
    advancedSettings: false,
    appearance: false,
    autofill: false,
    defaultBrowser: false,
    downloads: false,
    extensions: false,
    getMostChrome: false,
    languages: false,
    onStartup: false,
    people: false,
    performance: false,
    privacy: false,
    reset: false,
    safetyCheck: false,
    system: false,
  };
  // </if>
  // <if expr="is_chromeos">
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
      sidePanel: false,
    },
    advancedSettings: true,
    privacy: {
      searchPrediction: false,
      networkPrediction: false,
    },
    downloads: true,
    a11y: true,
    extensions: false,
    getMostChrome: false,
    languages: true,
    performance: false,
  };
  // </if>
}

export function setPageVisibilityForTesting(testVisibility: PageVisibility) {
  pageVisibility = testVisibility;
}
