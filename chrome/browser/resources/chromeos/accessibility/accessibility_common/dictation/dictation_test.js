// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/mock_accessibility_private.js']);

/**
 * Dictation feature using accessibility common extension browser tests.
 */
DictationE2ETest = class extends E2ETestBase {
  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/shell.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_switches.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
    ::switches::kEnableExperimentalAccessibilityDictationExtension);
  base::OnceClosure load_cb =
    base::BindOnce(&ash::AccessibilityManager::SetDictationEnabled,
        base::Unretained(ash::AccessibilityManager::Get()),
        true);
    `);
    super.testGenPreambleCommon('kAccessibilityCommonExtensionId');
  }
};

TEST_F('DictationE2ETest', 'SanityCheck', function() {
  this.newCallback(async () => {
    await importModule(
        'Dictation', '/accessibility_common/dictation/dictation.js');
    assertNotNullNorUndefined(Dictation);
  })();
});
