// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
  'common.js', '../../common/testing/assert_additions.js',
  '../../common/testing/e2e_test_base.js'
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
  #include "ash/accessibility/accessibility_delegate.h"
  #include "ash/shell.h"
  #include "base/bind.h"
  #include "base/callback.h"
  #include "chrome/browser/ash/accessibility/accessibility_manager.h"
  #include "chrome/common/extensions/extension_constants.h"
  #include "content/public/test/browser_test.h"
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
};

// TODO: wasm logs errors if it takes too long to load (e.g. liblouis wasm).
// Separately, LibLouis also logs errors.
// See https://crbug.com/1170991.
ChromeVoxE2ETest.prototype.failOnConsoleError = false;
