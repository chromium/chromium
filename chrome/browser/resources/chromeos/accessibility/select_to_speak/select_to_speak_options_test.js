// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);

/**
 * Browser tests for select-to-speak's options page.
 */
SelectToSpeakOptionsTest = class extends SelectToSpeakE2ETest {};

// Flaky. See crbug.com/1235944.
TEST_F('SelectToSpeakOptionsTest', 'DISABLED_RendersContent', function() {
  this.runWithLoadedOptionsPage(root => {
    // Sanity check the page renders correctly by looking for an item close or
    // at the very end of the page.
    const enableNavControlsSwitch =
        root.find({attributes: {name: 'Enable navigation controls'}});
    assertNotNullNorUndefined(enableNavControlsSwitch);
    assertEquals(
        chrome.automation.RoleType.SWITCH, enableNavControlsSwitch.role);
  });
});
