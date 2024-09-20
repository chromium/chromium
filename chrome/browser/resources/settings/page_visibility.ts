// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from './i18n_setup.js';

/**
 * Specifies page visibility based on incognito status and Chrome OS guest mode.
 */
export interface PageVisibility {
  a11y?: boolean;
  advancedSettings?: boolean;
  ai?: boolean;
  appearance?: boolean|AppearancePageVisibility;
  autofill?: boolean;
  defaultBrowser?: boolean;
  downloads?: boolean;
  extensions?: boolean;
  languages?: boolean;
  onStartup?: boolean;
  people?: boolean;
  performance?: boolean;
  privacy?: boolean|PrivacyPageVisibility;
  reset?: boolean;
  safetyCheck?: boolean;
  safetyHub?: boolean;
  system?: boolean;
}

export interface AppearancePageVisibility {
  bookmarksBar: boolean;
  homeButton: boolean;
  hoverCard: boolean;
  pageZoom: boolean;
  setTheme: boolean;
  sidePanel: boolean;
}

export interface PrivacyPageVisibility {
  networkPrediction: boolean;
  searchPrediction: boolean;
}

function createPageVisibility(): PageVisibility|undefined {
  if (!loadTimeData.getBoolean('isGuest')) {
    return undefined;
  }

  // <if expr="not is_chromeos">
  const pageVisibility = {
    a11y: false,
    advancedSettings: false,
    ai: false,
    appearance: false,
    autofill: false,
    defaultBrowser: false,
    downloads: false,
    extensions: false,
    languages: false,
    onStartup: false,
    people: false,
    performance: false,
    privacy: false,
    reset: false,
    safetyCheck: false,
    safetyHub: false,
    system: false,
  };
  // </if>
  // <if expr="is_chromeos">
  const pageVisibility = {
    ai: false,
    autofill: false,
    people: false,
    onStartup: false,
    reset: false,
    safetyCheck: false,
    safetyHub: false,
    appearance: {
      setTheme: false,
      homeButton: false,
      hoverCard: false,
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
    languages: true,
    performance: false,
  };
  // </if>

  return pageVisibility;
}

/**
 * Dictionary defining page visibility.
 */
export let pageVisibility: PageVisibility|undefined = createPageVisibility();

export function resetPageVisibilityForTesting(
    testVisibility: PageVisibility|undefined = createPageVisibility()) {
  pageVisibility = testVisibility;
}
