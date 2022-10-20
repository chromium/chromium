// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_next_e2e_test_base.js']);

/**
 * Test fixture for braille_command_data.js.
 */
ChromeVoxBrailleCommandDataTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Alphabetical based on file path.
    await importModule(
        'BrailleCommandData',
        '/chromevox/common/braille/braille_command_data.js');
  }
};


AX_TEST_F('ChromeVoxBrailleCommandDataTest', 'Duplicates', function() {
  try {
    BrailleCommandData.DOT_PATTERN_TO_COMMAND = [];
    BrailleCommandData.init_();
  } catch (e) {
    assertNotReached(e.toString());
  }
});
