// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

ChromeVoxBrailleTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    await Promise.all([
      // Alphabetized by file path.
      importModule(
          'BrailleBackground',
          '/chromevox/background/braille/braille_background.js'),
      importModule('LogStore', '/chromevox/background/logging/log_store.js'),
      importModule('LogType', '/chromevox/common/log_types.js'),
      importModule('SettingsManager', '/chromevox/common/settings_manager.js'),
      importModule('Spannable', '/chromevox/common/spannable.js'),
    ]);
  }
};

AX_TEST_F('ChromeVoxBrailleTest', 'BrailleLog', async function() {
  this.addCallbackPostMethod(
      LogStore.instance, 'writeTextLog', this.newCallback((text, type) => {
        assertEquals('Braille "test"', text);
        assertEquals(LogType.BRAILLE, type);
      }));
  SettingsManager.set('enableBrailleLogging', true);
  BrailleBackground.instance.frozen_ = false;
  BrailleBackground.instance.write({text: new Spannable('test')});
});
