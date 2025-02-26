// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for CaptionsHandler.
 */
ChromeVoxCaptionsHandlerTest = class extends ChromeVoxE2ETest {};

AX_TEST_F('ChromeVoxCaptionsHandlerTest', 'Open', function() {
  let liveCaptionEnabledCount = 0;
  chrome.accessibilityPrivate.enableLiveCaption = () =>
      liveCaptionEnabledCount++;

  // Simulate the preference being false beforehand.
  SettingsManager.getBoolean = () => false;

  CaptionsHandler.open();
  assertEquals(1, liveCaptionEnabledCount);

  // Simulate the preference being true beforehand.
  SettingsManager.getBoolean = () => true;

  liveCaptionEnabledCount = 0;
  CaptionsHandler.open();
  assertEquals(0, liveCaptionEnabledCount);
});
