// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Steps in the privacy guide flow in their order of appearance. The page
 * updates from those steps to show the corresponding page content.
 */
export enum PrivacyGuideStep {
  WELCOME = 'welcome',
  MSBB = 'msbb',
  HISTORY_SYNC = 'historySync',
  COOKIES = 'cookies',
  SAFE_BROWSING = 'safeBrowsing',
  SEARCH_SUGGESTIONS = 'searchSuggestions',
  PRELOAD = 'preload',
  COMPLETION = 'completion',
}

// TODO(crbug.com/1215630): remove this once PrivacyGuide3 is launched.
export enum PrivacyGuideStepPg3Off {
  WELCOME = 'welcome',
  MSBB = 'msbb',
  HISTORY_SYNC = 'historySync',
  SAFE_BROWSING = 'safeBrowsing',
  COOKIES = 'cookies',
  COMPLETION = 'completion',
}
