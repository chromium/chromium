// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

ChromeVoxBrailleTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Alphabetized by file path.
    await importModule(
        'BrailleBackground',
        '/chromevox/background/braille/braille_background.js');
    await importModule(
        'LogStore', '/chromevox/background/logging/log_store.js');
    await importModule('LogType', '/chromevox/common/log_types.js');
    await importModule(
        'SettingsManager', '/chromevox/common/settings_manager.js');
    await importModule('Spannable', '/chromevox/common/spannable.js');
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
