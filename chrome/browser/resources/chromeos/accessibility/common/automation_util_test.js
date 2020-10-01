// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../chromevox/testing/chromevox_next_e2e_test_base.js',
  'testing/assert_additions.js', '../chromevox/testing/snippets.js',
  'rect_util.js'
]);

/**
 * Test fixture for automation_util.js.
 */
AccessibilityExtensionAutomationUtilE2ETest =
    class extends ChromeVoxNextE2ETest {
  /** @override */
  setUp() {
    window.Dir = constants.Dir;
    window.RoleType = chrome.automation.RoleType;

    /** Filters nodes not rooted by desktop. */
    function filterNonDesktopRoot(node) {
      return node.root.role != RoleType.DESKTOP;
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


TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'GetAncestors', function() {
      this.runWithLoadedTree(this.basicDoc(), function(root) {
        let expectedLength = 1;
        while (root) {
          const ancestors = getNonDesktopAncestors(root);
          assertEquals(expectedLength++, ancestors.length);
          root = root.firstChild;
        }
      });
    });

TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'GetFirstAncestorWithRole',
    function() {
      this.runWithLoadedTree(
          `
    <div aria-label="x">
      <div aria-label="y">
        <p>
          <button>Hello world</div>
        </p>
      </div>
    </div>`,
          function(root) {
            const buttonNode = root.firstChild.firstChild.firstChild;
            const containerNode = AutomationUtil.getFirstAncestorWithRole(
                buttonNode, RoleType.GENERIC_CONTAINER);
            assertEquals(containerNode.name, 'y');

            const parentContainerNode = AutomationUtil.getFirstAncestorWithRole(
                containerNode, RoleType.GENERIC_CONTAINER);
            assertEquals(parentContainerNode.name, 'x');
          });
    });

TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'GetUniqueAncestors',
    function() {
      this.runWithLoadedTree(this.basicDoc(), function(root) {
        let leftmost = root, rightmost = root;
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

        assertEquals(
            1, getNonDesktopUniqueAncestors(leftmost, leftmost).length);
      }.bind(this));
    });

TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'GetDirection', function() {
      this.runWithLoadedTree(this.basicDoc(), function(root) {
        let left = root, right = root;

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
    });

TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'VisitContainer',
    function() {
      this.runWithLoadedTree(toolbarDoc(), function(r) {
        const pred = function(n) {
          return n.role != 'rootWebArea';
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
    });

TEST_F('AccessibilityExtensionAutomationUtilE2ETest', 'HitTest', function() {
  this.runWithLoadedTree(headingDoc, function(r) {
    const [h1, h2, a] = r.findAll({role: 'inlineTextBox'});

    assertEquals(h1, AutomationUtil.hitTest(r, RectUtil.center(h1.location)));
    assertEquals(
        h1, AutomationUtil.hitTest(r, RectUtil.center(h1.parent.location)));
    assertEquals(
        h1.parent.parent,
        AutomationUtil.hitTest(r, RectUtil.center(h1.parent.parent.location)));

    assertEquals(a, AutomationUtil.hitTest(r, RectUtil.center(a.location)));
    assertEquals(
        a, AutomationUtil.hitTest(r, RectUtil.center(a.parent.location)));
    assertEquals(
        a.parent.parent,
        AutomationUtil.hitTest(r, RectUtil.center(a.parent.parent.location)));
  });
});

TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'FindLastNodeSimple',
    function() {
      this.runWithLoadedTree(
          `<p aria-label=" "><div aria-label="x"></div></p>`, function(r) {
            assertEquals(
                'x',
                AutomationUtil
                    .findLastNode(
                        r,
                        function(n) {
                          return n.role == RoleType.GENERIC_CONTAINER;
                        })
                    .name);
          });
    });

TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'FindLastNodeNonLeaf',
    function() {
      this.runWithLoadedTree(
          `
    <div role="button" aria-label="outer">
      <div role="button" aria-label="inner">
      </div>
    </div>
    `,
          function(r) {
            assertEquals(
                'outer',
                AutomationUtil
                    .findLastNode(
                        r,
                        function(n) {
                          return n.role == RoleType.BUTTON;
                        })
                    .name);
          });
    });

TEST_F(
    'AccessibilityExtensionAutomationUtilE2ETest', 'FindLastNodeLeaf',
    function() {
      this.runWithLoadedTree(
          `
    <p>start</p>
    <div aria-label="outer"><div aria-label="inner"></div></div>
    <p>end</p>
    `,
          function(r) {
            assertEquals(
                'inner',
                AutomationUtil
                    .findLastNode(
                        r,
                        function(n) {
                          return n.role == RoleType.GENERIC_CONTAINER;
                        })
                    .name);
          });
    });
