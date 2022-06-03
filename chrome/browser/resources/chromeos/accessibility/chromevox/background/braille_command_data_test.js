// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_next_e2e_test_base.js']);

/**
 * Test fixture for braille_command_data.js.
 */
ChromeVoxBrailleCommandDataTest = class extends ChromeVoxNextE2ETest {};


SYNC_TEST_F('ChromeVoxBrailleCommandDataTest', 'Duplicates', function() {
  try {
    BrailleCommandData.DOT_PATTERN_TO_COMMAND = [];
    BrailleCommandData.init_();
  } catch (e) {
    assertNotReached(e.toString());
  }
});
