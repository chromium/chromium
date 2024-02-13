// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Placeholder file for mock app. Runs in an isolated guest.
 */

document.addEventListener('DOMContentLoaded', () => {
  if (document.location.pathname === '/background') {
    // In the background page, don't render the app.
    doBackgroundTasks();
    return;
  }

  // Note: This is intended to mimic how the real app initializes the search
  // index once on startup. But the real app does this in firstUpdated, not
  // setDelegate.
  initInAppSearchIndex();
});

/**
 * Do the background processing and then close the page.
 * Based on the internal version: go/help-app-internal-dobackgroundtasks. This
 * function's implementation should be kept up to date with the internal
 * version.
 */
async function doBackgroundTasks() {
  await window.customLaunchData.delegate.updateLauncherSearchIndex([{
    id: 'mock-app-test-id',
    title: 'Title',
    mainCategoryName: 'Help',
    tags: ['verycomplicatedsearchquery'],
    tagLocale: '',
    urlPathWithParameters: 'help/sub/3399763/',
    locale: '',
  }]);
  window.customLaunchData.delegate.closeBackgroundPage();
}

/**
 * Mimics the way the real app initializes the index for in-app search. Adds one
 * fake search result. The implementation is based on
 * go/help-app-internal-initInAppSearchIndex and should be kept up to date with
 * the internal version.
 */
async function initInAppSearchIndex() {
  await window.customLaunchData.delegate.clearSearchIndex();
  await window.customLaunchData.delegate.addOrUpdateSearchIndex([
    {
      id: 'mock-app-test-id',
      title: 'Get help with Chrome',
      body: 'Test body',
      mainCategoryName: 'Help',
      locale: 'en-US',
    },
  ]);
}
