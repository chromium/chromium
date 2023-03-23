// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for PanelBackground.
 */
ChromeVoxPanelBackgroundTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Alphabetical based on file path.
    await importModule(
        'PanelBackground', '/chromevox/background/panel/panel_background.js');
  }
};

AX_TEST_F('ChromeVoxPanelBackgroundTest', 'OnTutorialReady', async function() {
  const callbackPromise = new Promise(
      resolve => PanelBackground.instance.tutorialReadyCallback_ = resolve);

  assertFalse(PanelBackground.instance.tutorialReadyForTesting_);

  PanelBackground.instance.onTutorialReady_();
  await callbackPromise;

  assertTrue(PanelBackground.instance.tutorialReadyForTesting_);
});
