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
  SAFE_BROWSING = 'safeBrowsing',
  COOKIES = 'cookies',
  AD_TOPICS = 'adTopics',
  COMPLETION = 'completion',
}
