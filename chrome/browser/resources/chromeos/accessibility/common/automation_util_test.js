// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  'testing/assert_additions.js',
  'testing/common_e2e_test_base.js',
  'testing/snippets.js',
]);

/**
 * Test fixture for automation_util.js.
 */
AccessibilityExtensionAutomationUtilE2ETest = class extends CommonE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await Promise.all([
      importModule('RectUtil', '/common/rect_util.js'),
      importModule('AutomationUtil', '/common/automation_util.js'),
    ]);

    window.Dir = constants.Dir;
    window.RoleType = chrome.automation.RoleType;

    /** Filters nodes not rooted by desktop. */
    function filterNonDesktopRoot(node) {
      return node.root.role !== RoleType.DESKTOP;
    }

    window.getNonDesktopAncestors = function(node) {
      return AutomationUtil.getAncestors(node).filter(filterNonDesktopRoot);
    };

    window.getNonDesktopUniqueAncestors = function(node1, node2) {
      return AutomationUtil.getUniqueAncestors(node1, node2)
          .filter(filterNonDesktopRoot);
    };
  }

  basicDoc() {
    return `
  <p><a href='#'></a>hello</p>
  <h1><ul><li>a</ul><div role="group"><button></button></div></h1>
    `;
  }

  secondDoc() {
    return `
  <html>
  <head><title>Second doc</title></head>
  <body><div>Second</div></body>
  </html>
    `;
  }

  iframeDoc() {
    return `
  <html>
  <head><title>Second doc</title></head>
  <body>
    <iframe src="data:text/html,<p>Inside</p>"></iframe>
  </body>
  </html>
    `;
  }
};


AX_TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'GetAncestors',
    async function() {
      let current = await this.runWithLoadedTree(this.basicDoc());
      let expectedLength = 1;
      while (current) {
        const ancestors = getNonDesktopAncestors(current);
        assertEquals(expectedLength++, ancestors.length);
        current = current.firstChild;
      }
    });

AX_TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'GetFirstAncestorWithRole',
    async function() {
      const root = await this.runWithLoadedTree(`
    <div tabindex="0" aria-label="x">
      <div tabindex="0" aria-label="y">
        <p>
          <button>Hello world</div>
        </p>
      </div>
    </div>`);
      const buttonNode = root.firstChild.firstChild.firstChild;
      const containerNode = AutomationUtil.getFirstAncestorWithRole(
          buttonNode, RoleType.GENERIC_CONTAINER);
      assertEquals(containerNode.name, 'y');

      const parentContainerNode = AutomationUtil.getFirstAncestorWithRole(
          containerNode, RoleType.GENERIC_CONTAINER);
      assertEquals(parentContainerNode.name, 'x');
    });

AX_TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'GetUniqueAncestors',
    async function() {
      const root = await this.runWithLoadedTree(this.basicDoc());
      let leftmost = root;
      let rightmost = root;
      while (leftmost.firstChild) {
        leftmost = leftmost.firstChild;
      }
      while (rightmost.lastChild) {
        rightmost = rightmost.lastChild;
      }
      const leftAncestors = getNonDesktopAncestors(leftmost);
      const rightAncestors = getNonDesktopAncestors(rightmost);
      assertEquals(RoleType.LINK, leftmost.role);
      assertEquals(RoleType.BUTTON, rightmost.role);
      assertEquals(
          1, AutomationUtil.getDivergence(leftAncestors, rightAncestors));

      assertEquals(
          -1, AutomationUtil.getDivergence(leftAncestors, leftAncestors));

      const uniqueAncestorsLeft =
          getNonDesktopUniqueAncestors(rightmost, leftmost);
      const uniqueAncestorsRight =
          getNonDesktopUniqueAncestors(leftmost, rightmost);

      assertEquals(2, uniqueAncestorsLeft.length);
      assertEquals(RoleType.PARAGRAPH, uniqueAncestorsLeft[0].role);
      assertEquals(RoleType.LINK, uniqueAncestorsLeft[1].role);

      assertEquals(3, uniqueAncestorsRight.length);
      assertEquals(RoleType.HEADING, uniqueAncestorsRight[0].role);
      assertEquals(RoleType.GROUP, uniqueAncestorsRight[1].role);
      assertEquals(RoleType.BUTTON, uniqueAncestorsRight[2].role);

      assertEquals(1, getNonDesktopUniqueAncestors(leftmost, leftmost).length);
    });

AX_TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'GetDirection',
    async function() {
      const root = await this.runWithLoadedTree(this.basicDoc());
      let left = root;
      let right = root;

      // Same node.
      assertEquals(Dir.FORWARD, AutomationUtil.getDirection(left, right));

      // Ancestry.
      left = left.firstChild;
      // Upward movement is backward (in dfs).
      assertEquals(Dir.BACKWARD, AutomationUtil.getDirection(left, right));
      // Downward movement is forward.
      assertEquals(Dir.FORWARD, AutomationUtil.getDirection(right, left));

      // Ordered.
      right = right.lastChild;
      assertEquals(Dir.BACKWARD, AutomationUtil.getDirection(right, left));
      assertEquals(Dir.FORWARD, AutomationUtil.getDirection(left, right));
    });

AX_TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'VisitContainer',
    async function() {
      const r = await this.runWithLoadedTree(toolbarDoc());
      const pred = function(n) {
        return n.role !== 'rootWebArea';
      };

      const toolbar = AutomationUtil.findNextNode(r, 'forward', pred);
      assertEquals('toolbar', toolbar.role);

      const back = AutomationUtil.findNextNode(toolbar, 'forward', pred);
      assertEquals('Back', back.name);
      assertEquals(
          toolbar, AutomationUtil.findNextNode(back, 'backward', pred));

      const forward = AutomationUtil.findNextNode(back, 'forward', pred);
      assertEquals('Forward', forward.name);
      assertEquals(
          back, AutomationUtil.findNextNode(forward, 'backward', pred));
    });

AX_TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'HitTest', async function() {
      const r = await this.runWithLoadedTree(headingDoc);
      const [h1, h2, a] = r.findAll({role: 'inlineTextBox'});

      assertEquals(h1, AutomationUtil.hitTest(r, RectUtil.center(h1.location)));
      assertEquals(
          h1, AutomationUtil.hitTest(r, RectUtil.center(h1.parent.location)));
      assertEquals(
          h1.parent.parent,
          AutomationUtil.hitTest(
              r, RectUtil.center(h1.parent.parent.location)));

      assertEquals(a, AutomationUtil.hitTest(r, RectUtil.center(a.location)));
      assertEquals(
          a, AutomationUtil.hitTest(r, RectUtil.center(a.parent.location)));
      assertEquals(
          a.parent.parent,
          AutomationUtil.hitTest(r, RectUtil.center(a.parent.parent.location)));
    });

AX_TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'FindLastNodeSimple',
    async function() {
      const r = await this.runWithLoadedTree(
          `<p aria-label=" "><div tabindex="0" aria-label="x"></div></p>`);
      assertEquals(
          'x',
          AutomationUtil
              .findLastNode(r, n => n.role === RoleType.GENERIC_CONTAINER)
              .name);
    });

AX_TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'FindLastNodeNonLeaf',
    async function() {
      const r = await this.runWithLoadedTree(`
    <div role="button" aria-label="outer">
      <div role="heading" aria-label="inner">
      </div>
    </div>
    `);
      assertEquals(
          'outer',
          AutomationUtil.findLastNode(r, n => n.role === RoleType.BUTTON).name);
    });

AX_TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'FindLastNodeLeaf',
    async function() {
      const r = await this.runWithLoadedTree(`
    <p>start</p>
    <div aria-label="outer"><div tabindex="0" aria-label="inner"></div></div>
    <p>end</p>
    `);
      assertEquals(
          'inner',
          AutomationUtil
              .findLastNode(r, n => n.role === RoleType.GENERIC_CONTAINER)
              .name);
    });
