// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for braille_command_data.js.
 */
ChromeVoxMV2BrailleCommandDataTest = class extends ChromeVoxE2ETest {};


AX_TEST_F('ChromeVoxMV2BrailleCommandDataTest', 'Duplicates', function() {
  try {
    BrailleCommandData.DOT_PATTERN_TO_COMMAND = [];
    BrailleCommandData.init();
  } catch (e) {
    assertNotReached(e.toString());
  }
});
