// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
  'common.js',
  '../../common/testing/assert_additions.js',
  '../../common/testing/e2e_test_base.js',
]);

/**
 * Base test fixture for ChromeVox end to end tests.
 * These tests run against production ChromeVox inside of the extension's
 * background page context.
 */
ChromeVoxE2ETest = class extends E2ETestBase {
  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
  #include "extensions/common/extension_l10n_util.h"
      `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`
    auto allow = extension_l10n_util::AllowGzippedMessagesAllowedForTest();
    base::OnceClosure load_cb =
        base::BindOnce(&ash::AccessibilityManager::EnableSpokenFeedback,
            base::Unretained(ash::AccessibilityManager::Get()),
            true);
      `);

    super.testGenPreambleCommon(
        'kChromeVoxExtensionId', ChromeVoxE2ETest.prototype.failOnConsoleError);
  }

  /**@override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Alphabetical based on file path.
    await importModule(
        'BrailleInterface',
        '/chromevox/background/braille/braille_interface.js');
    await importModule('ChromeVox', '/chromevox/background/chromevox.js');
    await importModule(
        'ChromeVoxState', '/chromevox/background/chromevox_state.js');
    await importModule(
        'NavBraille', '/chromevox/common/braille/nav_braille.js');
    await importModule(
        ['AbstractEarcons', 'Earcon'], '/chromevox/common/abstract_earcons.js');
    await importModule('TtsInterface', '/chromevox/common/tts_interface.js');
    await importModule('QueueMode', '/chromevox/common/tts_types.js');

    await ChromeVoxState.ready();
  }
};

// TODO: wasm logs errors if it takes too long to load (e.g. liblouis wasm).
// Separately, LibLouis also logs errors.
// See https://crbug.com/1170991.
ChromeVoxE2ETest.prototype.failOnConsoleError = false;
