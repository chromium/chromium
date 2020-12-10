// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

/**
 * Specifies page visibility based on incognito status and Chrome OS guest mode.
 * @typedef {{
 *   a11y: (boolean|undefined),
 *   advancedSettings: (boolean|undefined),
 *   appearance: (boolean|undefined|AppearancePageVisibility),
 *   autofill: (boolean|undefined),
 *   defaultBrowser: (boolean|undefined),
 *   downloads: (boolean|undefined),
 *   languages: (boolean|undefined),
 *   onStartup: (boolean|undefined),
 *   people: (boolean|undefined),
 *   privacy: (boolean|undefined|PrivacyPageVisibility),
 *   reset: (boolean|undefined),
 * }}
 */
export let PageVisibility;

/**
 * @typedef {{
 *   bookmarksBar: boolean,
 *   homeButton: boolean,
 *   pageZoom: boolean,
 *   setTheme: boolean,
 * }}
 */
export let AppearancePageVisibility;

/**
 * @typedef {{
 *   networkPrediction: boolean,
 *   searchPrediction: boolean,
 * }}
 */
export let PrivacyPageVisibility;

/**
 * Dictionary defining page visibility.
 * @type {!PageVisibility}
 */
export let pageVisibility;

if (loadTimeData.getBoolean('isGuest')) {
  // "if not chromeos" and "if chromeos" in two completely separate blocks
  // to work around closure compiler.
  // <if expr="not chromeos">
  pageVisibility = {
    autofill: false,
    people: false,
    privacy: false,
    onStartup: false,
    reset: false,
    safetyCheck: false,
    appearance: false,
    defaultBrowser: false,
    advancedSettings: false,
    extensions: false,
    languages: false,
  };
  // </if>
  // <if expr="chromeos">
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
} else {
  // All pages are visible when not in chromeos. Since polymer only notifies
  // after a property is set.
  // <if expr="chromeos">
  pageVisibility = {
    autofill: true,
    people: true,
    onStartup: true,
    reset: true,
    safetyCheck: true,
    appearance: {
      setTheme: true,
      homeButton: true,
      bookmarksBar: true,
      pageZoom: true,
    },
    advancedSettings: true,
    privacy: {
      searchPrediction: true,
      networkPrediction: true,
    },
    downloads: true,
    a11y: true,
    extensions: true,
    languages: true,
  };
  // </if>
}

export function setPageVisibilityForTesting(testVisibility) {
  pageVisibility = testVisibility;
}
