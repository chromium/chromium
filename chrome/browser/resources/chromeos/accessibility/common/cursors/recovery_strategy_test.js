// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/common_e2e_test_base.js']);

/**
 * Test fixture for recovery strategy tests.
 */
AccessibilityExtensionRecoveryStrategyTest = class extends CommonE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule(
        [
          'RecoveryStrategy',
          'AncestryRecoveryStrategy',
          'TreePathRecoveryStrategy',
        ],
        '/common/cursors/recovery_strategy.js');
    globalThis.RoleType = chrome.automation.RoleType;
  }
};


// TODO(https://issuetracker.google.com/issues/263127143) Recovery can likely be
// simplified now that most ids are stable.
AX_TEST_F(
    'AccessibilityExtensionRecoveryStrategyTest', 'ReparentedRecovery',
    async function() {
      const root = await this.runWithLoadedTree(`
    <input type="text"></input>
    <p id="p">hi</p>
    <button id="go"</button>
    <script>
      document.getElementById('go').addEventListener('click', function() {
        let p = document.getElementById('p');
        p.remove();
        document.body.appendChild(p);
      });
    </script>
  `);
      const p = root.find({role: RoleType.PARAGRAPH});
      const s = root.find({role: RoleType.STATIC_TEXT});
      const b = root.find({role: RoleType.BUTTON});
      const bAncestryRecovery = new AncestryRecoveryStrategy(b);
      const pAncestryRecovery = new AncestryRecoveryStrategy(p);
      const sAncestryRecovery = new AncestryRecoveryStrategy(s);
      const bTreePathRecovery = new TreePathRecoveryStrategy(b);
      const pTreePathRecovery = new TreePathRecoveryStrategy(p);
      const sTreePathRecovery = new TreePathRecoveryStrategy(s);

      b.doDefault();
      await this.waitForEvent(b, 'clicked');
      assertFalse(
          bAncestryRecovery.requiresRecovery(),
          'bAncestryRecovery.requiresRecovery');
      assertFalse(
          pAncestryRecovery.requiresRecovery(),
          'pAncestryRecovery.requiresRecovery()');
      assertFalse(
          sAncestryRecovery.requiresRecovery(),
          'sAncestryRecovery.requiresRecovery()');
      assertFalse(
          bTreePathRecovery.requiresRecovery(),
          'bTreePathRecovery.requiresRecovery()');
      assertFalse(
          pTreePathRecovery.requiresRecovery(),
          'pTreePathRecovery.requiresRecovery()');
      assertFalse(
          sTreePathRecovery.requiresRecovery(),
          'sTreePathRecovery.requiresRecovery()');
      assertFalse(
          bAncestryRecovery.requiresRecovery(),
          'bAncestryRecovery.requiresRecovery()');
      assertFalse(
          pAncestryRecovery.requiresRecovery(),
          'pAncestryRecovery.requiresRecovery()');
      assertFalse(
          sAncestryRecovery.requiresRecovery(),
          'sAncestryRecovery.requiresRecovery()');
      assertFalse(
          bTreePathRecovery.requiresRecovery(),
          'bTreePathRecovery.requiresRecovery()');
      assertFalse(
          pTreePathRecovery.requiresRecovery(),
          'pTreePathRecovery.requiresRecovery()');
      assertFalse(
          sTreePathRecovery.requiresRecovery(),
          'sTreePathRecovery.requiresRecovery()');
    });
