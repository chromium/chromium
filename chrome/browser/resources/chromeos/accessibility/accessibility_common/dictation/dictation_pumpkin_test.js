// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['dictation_test_base.js']);

/**
 * Dictation test for Pumpkin semantic parsing into action macros.
 */
DictationPumpkinTest = class extends DictationE2ETestBase {
  constructor() {
    super();
  }

  /**
   * Sets up Dictation with commands enabled.
   */
  async waitForDictationWithPumpkin() {
    await this.waitForDictationModule();
    await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'en-US');
    await this.setCommandsEnabledForTest(true);
  }

  /**
   * Gets the SpeechParser.
   */
  getSpeechParser() {
    return accessibilityCommon.dictation_.speechParser_;
  }
};

// This test can serve as an example of how to test Pumpkin parsing from
// javascript without mocking out methods to allow macro execution.
// TODO(crbug.com/1264544): This test flakily fails because of a memory issue
// when loading Pumpkin. The issue is not present in google3 test of Pumpkin
// WASM or when running Chrome with Dictation, so it is likely a limitation in
// the Chrome test framework. The test is only run when the default-false gn
// arg, enable_pumpkin_for_dictation, is set to true.
SYNC_TEST_F('DictationPumpkinTest', 'DeleteCommand', async function() {
  await this.waitForDictationWithPumpkin();
  const macro = await this.getSpeechParser().parse('delete two characters');
  assertEquals(macro.getMacroNameString(), 'DELETE_PREV_CHAR');
});