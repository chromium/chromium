// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['e2e_test_base.js']);

/**
 * @fileoverview Test base used both by shared files (in common/) and the
 * accessibility common extension (in accessibility_common/).
 */
CommonE2ETestBase = class extends E2ETestBase {
  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    // Note that at least one accessibility common feature has to be enabled for
    // the extension to load. Extension load is required for this test suite to
    // have a place to be injected.
    GEN(`
  base::OnceClosure load_cb =
      base::BindOnce(&ash::AccessibilityManager::EnableAutoclick,
          base::Unretained(ash::AccessibilityManager::Get()),
          true);
    `);
    super.testGenPreambleCommon('kAccessibilityCommonExtensionId');
  }
};
