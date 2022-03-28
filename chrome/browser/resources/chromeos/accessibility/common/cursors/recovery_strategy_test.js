// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '//chrome/browser/resources/chromeos/accessibility/chromevox/testing/chromevox_next_e2e_test_base.js',
]);

/**
 * Test fixture for recovery strategy tests.
 */
AccessibilityExtensionRecoveryStrategyTest =
    class extends ChromeVoxNextE2ETest {
  constructor() {
    super();
  }
};


TEST_F(
    'AccessibilityExtensionRecoveryStrategyTest', 'ReparentedRecovery',
    function() {
      this.runWithLoadedTree(
          `
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
  `,
          function(root) {
            const p = root.find({role: RoleType.PARAGRAPH});
            const s = root.find({role: RoleType.STATIC_TEXT});
            const b = root.find({role: RoleType.BUTTON});
            const bAncestryRecovery = new AncestryRecoveryStrategy(b);
            const pAncestryRecovery = new AncestryRecoveryStrategy(p);
            const sAncestryRecovery = new AncestryRecoveryStrategy(s);
            const bTreePathRecovery = new TreePathRecoveryStrategy(b);
            const pTreePathRecovery = new TreePathRecoveryStrategy(p);
            const sTreePathRecovery = new TreePathRecoveryStrategy(s);
            this.listenOnce(b, 'clicked', function() {
              assertFalse(
                  bAncestryRecovery.requiresRecovery(),
                  'bAncestryRecovery.requiresRecovery');
              assertTrue(
                  pAncestryRecovery.requiresRecovery(),
                  'pAncestryRecovery.requiresRecovery()');
              assertTrue(
                  sAncestryRecovery.requiresRecovery(),
                  'sAncestryRecovery.requiresRecovery()');
              assertFalse(
                  bTreePathRecovery.requiresRecovery(),
                  'bTreePathRecovery.requiresRecovery()');
              assertTrue(
                  pTreePathRecovery.requiresRecovery(),
                  'pTreePathRecovery.requiresRecovery()');
              assertTrue(
                  sTreePathRecovery.requiresRecovery(),
                  'sTreePathRecovery.requiresRecovery()');

              assertEquals(RoleType.BUTTON, bAncestryRecovery.node.role);
              assertEquals(root, pAncestryRecovery.node);
              assertEquals(root, sAncestryRecovery.node);

              assertEquals(b, bTreePathRecovery.node);
              assertEquals(b, pTreePathRecovery.node);
              assertEquals(b, sTreePathRecovery.node);

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
            // Trigger the change.
            b.doDefault();
          });
    });
