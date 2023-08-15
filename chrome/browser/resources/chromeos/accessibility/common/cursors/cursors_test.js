// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/common_e2e_test_base.js']);

/**
 * Test fixture for cursors.
 */
AccessibilityExtensionCursorsTest = class extends CommonE2ETestBase {
  /** Test Cursor. @const {string} */
  get CURSOR() {
    return 'cursor';
  }

  /** Test CursorRange. @const {string} */
  get RANGE() {
    return 'range';
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await Promise.all([
      importModule('CursorRange', '/common/cursors/range.js'),
      importModule(
          ['Cursor', 'CursorMovement', 'CursorUnit', 'WrappingCursor'],
          '/common/cursors/cursor.js'),

      importModule('AutomationUtil', '/common/automation_util.js'),
      importModule('AutomationPredicate', '/common/automation_predicate.js'),
      importModule('constants', '/common/constants.js'),
      importModule('createMockNode', '/common/testing/test_node_generator.js'),
    ]);
    // Various aliases
    globalThis.CHARACTER = CursorUnit.CHARACTER;
    globalThis.WORD = CursorUnit.WORD;
    globalThis.LINE = CursorUnit.LINE;
    globalThis.NODE = CursorUnit.NODE;
    globalThis.BOUND = CursorMovement.BOUND;
    globalThis.DIRECTIONAL = CursorMovement.DIRECTIONAL;
    globalThis.SYNC = CursorMovement.SYNC;
    globalThis.BACKWARD = constants.Dir.BACKWARD;
    globalThis.FORWARD = constants.Dir.FORWARD;
    globalThis.RoleType = chrome.automation.RoleType;
  }

  /**
   * Performs a series of operations on a cursor and asserts the result.
   * @param {Cursor} cursor The starting cursor.
   * @param {!Array<Array<
   *          CursorUnit|
   *          CursorMovement|
   *          constants.Dir|
   *          {index: number, value: string}>>} moves An array of arrays. Each
   *     inner array contains 4 items: unit, movement, direction, and assertions
   *     object. See example below.
   */
  cursorMoveAndAssert(cursor, moves) {
    let move = null;
    while (move = moves.shift()) {
      cursor = cursor.move(move[0], move[1], move[2]);
      const expected = move[3];
      this.makeCursorAssertion(expected, cursor);
    }
  }

  /**
   * Performs a series of operations on a range and asserts the result.
   * @param {CursorRange} range The starting range.
   * @param {!Array<Array<
   *         CursorUnit|
   *         constants.Dir|
   *         {index: number, value: string}>>} moves An array of arrays. Each
   *     inner array contains 4 items: unit, direction, start and end assertions
   *     objects. See example below.
   */
  rangeMoveAndAssert(range, moves) {
    let move = null;
    while (move = moves.shift()) {
      range = range.move(move[0], move[1]);
      const expectedStart = move[2];
      const expectedEnd = move[3];

      this.makeCursorAssertion(expectedStart, range.start);
      this.makeCursorAssertion(expectedEnd, range.end);
    }
  }

  /**
   * Makes assertions about the given |cursor|.
   * @param {{index: number, value: string}} expected
   * @param {Cursor} cursor
   */
  makeCursorAssertion(expected, cursor) {
    if (expected.value !== undefined) {
      assertEquals(expected.value, cursor.node.name);
    }
    if (expected.index !== undefined) {
      assertEquals(expected.index, cursor.index,
        'wrong index at ' + expected.value + ', expected: ' + expected.index + ' actual: ' + cursor.index);
    }
  }

  /**
   * Runs the specified moves on the |doc| and asserts expectations.
   * @param {function} doc
   * @param {!Array<Array<
   *          CursorUnit|
   *          CursorMovement|
   *          constants.Dir|
   *          {index: number, value: string}>>} moves An array of arrays. Each
   *     inner array contains 4 items: see |cursorMoveAndAssert| and
   *     |rangeMoveAndAssert|.
   * @param {string=} opt_testType Either CURSOR or RANGE.
   */
  async runCursorMovesOnDocument(doc, moves, opt_testType) {
    const root = await this.runWithLoadedTree(doc);
    let start = null;

    // This occurs as a result of a load complete.
    start =
        AutomationUtil.findNodePost(root, FORWARD, AutomationPredicate.leaf);

    const cursor = new Cursor(start, 0);
    if (!opt_testType || opt_testType === this.CURSOR) {
      const cursor = new Cursor(start, 0);
      this.cursorMoveAndAssert(cursor, moves);
    } else if (opt_testType === this.RANGE) {
      const range = new CursorRange(cursor, cursor);
      this.rangeMoveAndAssert(range, moves);
    }
  }

  get simpleDoc() {
    return `
      <p>start <span>same line</span>
      <p>end
    `;
  }

  get multiInlineDoc() {
    return `
      <p style='max-width: 5px'>start diff line</p>
      <p>end
    `;
  }

  get buttonAndInlineTextDoc() {
    return `
      <div>Inline text content</div>
      <div role="button">Button example content</div>
    `;
  }
};


AX_TEST_F(
    'AccessibilityExtensionCursorsTest', 'CharacterCursor', async function() {
      await this.runCursorMovesOnDocument(this.simpleDoc, [
        [CHARACTER, DIRECTIONAL, FORWARD, {index: 1, value: 'start '}],
        [CHARACTER, DIRECTIONAL, BACKWARD, {index: 0, value: 'start '}],
        // Bumps up against edge.
        [CHARACTER, DIRECTIONAL, BACKWARD, {index: 0, value: 'start '}],

        [CHARACTER, DIRECTIONAL, FORWARD, {index: 1, value: 'start '}],
        [CHARACTER, DIRECTIONAL, FORWARD, {index: 2, value: 'start '}],
        [CHARACTER, DIRECTIONAL, FORWARD, {index: 3, value: 'start '}],
        [CHARACTER, DIRECTIONAL, FORWARD, {index: 4, value: 'start '}],
        [CHARACTER, DIRECTIONAL, FORWARD, {index: 5, value: 'start '}],

        [CHARACTER, DIRECTIONAL, FORWARD, {index: 0, value: 'same line'}],
        [CHARACTER, DIRECTIONAL, FORWARD, {index: 1, value: 'same line'}],
        [CHARACTER, DIRECTIONAL, BACKWARD, {index: 0, value: 'same line'}],

        [CHARACTER, DIRECTIONAL, BACKWARD, {index: 5, value: 'start '}],
      ]);
    });

AX_TEST_F('AccessibilityExtensionCursorsTest', 'WordCursor', async function() {
  await this.runCursorMovesOnDocument(this.simpleDoc, [
    // Word (BOUND).
    [WORD, BOUND, BACKWARD, {index: 0, value: 'start '}],
    [WORD, BOUND, BACKWARD, {index: 0, value: 'start '}],
    [WORD, BOUND, FORWARD, {index: 5, value: 'start '}],
    [WORD, BOUND, FORWARD, {index: 5, value: 'start '}],

    // Word (DIRECTIONAL).
    [WORD, DIRECTIONAL, FORWARD, {index: 0, value: 'same line'}],
    [WORD, DIRECTIONAL, FORWARD, {index: 5, value: 'same line'}],

    [WORD, DIRECTIONAL, FORWARD, {index: 0, value: 'end'}],
    [WORD, DIRECTIONAL, FORWARD, {index: 0, value: 'end'}],

    [WORD, DIRECTIONAL, BACKWARD, {index: 5, value: 'same line'}],
    [WORD, DIRECTIONAL, BACKWARD, {index: 0, value: 'same line'}],

    [WORD, DIRECTIONAL, BACKWARD, {index: 0, value: 'start '}],
    [WORD, DIRECTIONAL, BACKWARD, {index: 0, value: undefined}],
  ]);
});

AX_TEST_F(
    'AccessibilityExtensionCursorsTest', 'CharacterWordCursor',
    async function() {
      await this.runCursorMovesOnDocument(this.simpleDoc, [
        [CHARACTER, DIRECTIONAL, FORWARD, {index: 1, value: 'start '}],

        [WORD, DIRECTIONAL, FORWARD, {index: 0, value: 'same line'}],
        [CHARACTER, DIRECTIONAL, FORWARD, {index: 1, value: 'same line'}],
        [WORD, DIRECTIONAL, FORWARD, {index: 5, value: 'same line'}],
        [CHARACTER, DIRECTIONAL, BACKWARD, {index: 4, value: 'same line'}],
        [WORD, DIRECTIONAL, FORWARD, {index: 5, value: 'same line'}],
        [CHARACTER, DIRECTIONAL, FORWARD, {index: 6, value: 'same line'}],
        [WORD, DIRECTIONAL, BACKWARD, {index: 0, value: 'same line'}],
        [CHARACTER, DIRECTIONAL, BACKWARD, {index: 5, value: 'start '}],
        [CHARACTER, DIRECTIONAL, BACKWARD, {index: 4, value: 'start '}],
        [WORD, DIRECTIONAL, BACKWARD, {index: 0, value: 'start '}],
      ]);
    });

AX_TEST_F('AccessibilityExtensionCursorsTest', 'LineCursor', async function() {
  await this.runCursorMovesOnDocument(this.simpleDoc, [
    // Line (BOUND).
    [LINE, BOUND, FORWARD, {value: 'same line'}],
    [LINE, BOUND, FORWARD, {value: 'same line'}],
    [LINE, BOUND, BACKWARD, {value: 'start '}],
    [LINE, BOUND, BACKWARD, {value: 'start '}],

    // Line (DIRECTIONAL).
    [LINE, DIRECTIONAL, FORWARD, {value: 'end'}],
    [LINE, DIRECTIONAL, FORWARD, {value: 'end'}],
    [LINE, DIRECTIONAL, BACKWARD, {value: 'same line'}],
    // Bump against an edge.
    [LINE, DIRECTIONAL, BACKWARD, {value: 'same line'}],
    [LINE, BOUND, BACKWARD, {value: 'start '}],
  ]);
});

AX_TEST_F('AccessibilityExtensionCursorsTest', 'SyncCursor', async function() {
  await this.runCursorMovesOnDocument(this.simpleDoc, [
    [WORD, SYNC, FORWARD, {index: 0, value: 'start '}],

    [NODE, DIRECTIONAL, FORWARD, {value: 'same line'}],
    [CHARACTER, SYNC, FORWARD, {index: 0, value: 'same line'}],

    [NODE, DIRECTIONAL, BACKWARD, {value: 'start '}],
    [CHARACTER, SYNC, BACKWARD, {index: 5, value: 'start '}],

    [NODE, DIRECTIONAL, FORWARD, {value: 'same line'}],
    [WORD, SYNC, BACKWARD, {index: 5, value: 'same line'}],
  ]);
});

AX_TEST_F(
    'AccessibilityExtensionCursorsTest', 'CharacterRange', async function() {
      await this.runCursorMovesOnDocument(
          this.simpleDoc,
          [
            [
              CHARACTER,
              FORWARD,
              {value: 'start ', index: 1},
              {value: 'start ', index: 2},
            ],
            [
              CHARACTER,
              FORWARD,
              {value: 'start ', index: 2},
              {value: 'start ', index: 3},
            ],
            [
              CHARACTER,
              FORWARD,
              {value: 'start ', index: 3},
              {value: 'start ', index: 4},
            ],
            [
              CHARACTER,
              FORWARD,
              {value: 'start ', index: 4},
              {value: 'start ', index: 5},
            ],
            [
              CHARACTER,
              FORWARD,
              {value: 'start ', index: 5},
              {value: 'start ', index: 6},
            ],

            [
              CHARACTER,
              FORWARD,
              {value: 'same line', index: 0},
              {value: 'same line', index: 1},
            ],

            [
              CHARACTER,
              BACKWARD,
              {value: 'start ', index: 5},
              {value: 'start ', index: 6},
            ],
            [
              CHARACTER,
              BACKWARD,
              {value: 'start ', index: 4},
              {value: 'start ', index: 5},
            ],
            [
              CHARACTER,
              BACKWARD,
              {value: 'start ', index: 3},
              {value: 'start ', index: 4},
            ],
            [
              CHARACTER,
              BACKWARD,
              {value: 'start ', index: 2},
              {value: 'start ', index: 3},
            ],
            [
              CHARACTER,
              BACKWARD,
              {value: 'start ', index: 1},
              {value: 'start ', index: 2},
            ],
            [
              CHARACTER,
              BACKWARD,
              {value: 'start ', index: 0},
              {value: 'start ', index: 1},
            ],
            [
              CHARACTER,
              BACKWARD,
              {value: undefined, index: 0},
              {value: undefined, index: 1},
            ],
          ],
          this.RANGE);
    });

AX_TEST_F('AccessibilityExtensionCursorsTest', 'WordRange', async function() {
  await this.runCursorMovesOnDocument(
      this.simpleDoc,
      [
        [
          WORD,
          FORWARD,
          {value: 'same line', index: 0},
          {value: 'same line', index: 4},
        ],
        [
          WORD,
          FORWARD,
          {value: 'same line', index: 5},
          {value: 'same line', index: 9},
        ],

        [WORD, FORWARD, {value: 'end', index: 0}, {value: 'end', index: 3}],
        [WORD, FORWARD, {value: 'end', index: 0}, {value: 'end', index: 3}],

        [
          WORD,
          BACKWARD,
          {value: 'same line', index: 5},
          {value: 'same line', index: 9},
        ],
        [
          WORD,
          BACKWARD,
          {value: 'same line', index: 0},
          {value: 'same line', index: 4},
        ],

        [
          WORD,
          BACKWARD,
          {value: 'start ', index: 0},
          {value: 'start ', index: 5},
        ],
        [
          WORD,
          BACKWARD,
          {value: undefined, index: 0},
          {value: undefined, index: 5},
        ],
      ],
      this.RANGE);
});


AX_TEST_F('AccessibilityExtensionCursorsTest', 'LineRange', async function() {
  await this.runCursorMovesOnDocument(
      this.simpleDoc,
      [
        [LINE, FORWARD, {value: 'end', index: 0}, {value: 'end', index: 3}],
        [LINE, FORWARD, {value: 'end', index: 0}, {value: 'end', index: 3}],
        [LINE, FORWARD, {value: 'end', index: 0}, {value: 'end', index: 3}],
        [LINE, FORWARD, {value: 'end', index: 0}, {value: 'end', index: 3}],

        [
          LINE,
          BACKWARD,
          {value: 'start ', index: 0},
          {value: 'same line', index: 9},
        ],

        [
          LINE,
          BACKWARD,
          {value: 'start ', index: 0},
          {value: 'same line', index: 9},
        ],
      ],
      this.RANGE);
});

AX_TEST_F(
    'AccessibilityExtensionCursorsTest', 'DontSplitOnNodeNavigation',
    async function() {
      const root = await this.runWithLoadedTree(this.multiInlineDoc);
      const para = root.firstChild;
      assertEquals('paragraph', para.role);
      let cursor = new Cursor(para.firstChild, 0);
      cursor = cursor.move(NODE, DIRECTIONAL, FORWARD);
      assertEquals('staticText', cursor.node.role);
      assertEquals('end', cursor.node.name);

      cursor = cursor.move(NODE, DIRECTIONAL, BACKWARD);
      assertEquals('staticText', cursor.node.role);
      assertEquals('start diff line', cursor.node.name);

      assertEquals('inlineTextBox', cursor.node.firstChild.role);
      assertEquals('start ', cursor.node.firstChild.name);
      assertEquals('diff ', cursor.node.firstChild.nextSibling.name);
      assertEquals('line', cursor.node.lastChild.name);
    });

AX_TEST_F(
    'AccessibilityExtensionCursorsTest', 'WrappingCursors', async function() {
      const root = await this.runWithLoadedTree(this.multiInlineDoc);
      const first = root;
      const last = root.lastChild.firstChild;
      let cursor = new WrappingCursor(first, -1);

      // Wrap from first node to last node.
      cursor = cursor.move(NODE, DIRECTIONAL, BACKWARD);
      assertEquals(last, cursor.node);

      // Wrap from last node to first node.
      cursor = cursor.move(NODE, DIRECTIONAL, FORWARD);
      assertEquals(first, cursor.node);
    });

AX_TEST_F(
    'AccessibilityExtensionCursorsTest', 'IsInWebRange', async function() {
      const root = await this.runWithLoadedTree(this.simpleDoc);
      const para = root.firstChild;
      const webRange = CursorRange.fromNode(para);
      const auraRange = CursorRange.fromNode(root.parent);
      assertFalse(auraRange.isWebRange());
      assertTrue(webRange.isWebRange());
    });

// Disabled due to being flaky on ChromeOS. See https://crbug.com/1227435.
AX_TEST_F(
    'AccessibilityExtensionCursorsTest', 'DISABLED_SingleDocSelection',
    async function() {
      const root = await this.runWithLoadedTree(`
    <span>start</span>
    <p><a href="google.com">google home page</a></p>
    <p>some more text</p>
    <p>end of text</p>
  `);
      // For some reason, Blink fails if we don't first select something
      // on the page.
      CursorRange.fromNode(root).select();
      const link = root.find({role: RoleType.LINK});
      const p1 = root.find({role: RoleType.PARAGRAPH});
      const p2 = p1.nextSibling;

      const singleSel =
          new CursorRange(new Cursor(link, 0), new Cursor(link, 1));

      const multiSel = new CursorRange(
          new Cursor(p1.firstChild, 2), new Cursor(p2.firstChild, 4));

      function verifySel() {
        if (root.selectionStartObject === link.firstChild) {
          assertEquals(link.firstChild, root.selectionStartObject);
          assertEquals(0, root.selectionStartOffset);
          assertEquals(link.firstChild, root.selectionEndObject);
          assertEquals(1, root.selectionEndOffset);
          this.listenOnce(root, 'textSelectionChanged', verifySel);
          multiSel.select();
        } else if (root.selectionStartObject === p1.firstChild) {
          assertEquals(p1.firstChild, root.selectionStartObject);
          assertEquals(2, root.selectionStartOffset);
          assertEquals(p2.firstChild, root.selectionEndObject);
          assertEquals(4, root.selectionEndOffset);
        }
      }

      this.listenOnce(root, 'textSelectionChanged', verifySel, true);
      singleSel.select();
    });

AX_TEST_F(
    'AccessibilityExtensionCursorsTest', 'InlineElementOffset',
    async function() {
      const root = await this.runWithLoadedTree(`
    <span>start</span>
    <p>This<br> is a<a href="#g">test</a>of selection</p>
  `);
      root.addEventListener(
          'documentSelectionChanged', this.newCallback(function(evt) {
            // Test setup moves initial focus; ensure we don't test that here.
            if (testNode !== root.selectionStartObject) {
              return;
            }

            // This is a little unexpected though not really incorrect; Ctrl+C
            // works.
            assertEquals(testNode, root.selectionStartObject);
            assertEquals(ofSelectionNode, root.selectionEndObject);
            assertEquals(4, root.selectionStartOffset);
            assertEquals(1, root.selectionEndOffset);
          }));

      // This is the link's static text.
      const testNode = root.lastChild.lastChild.previousSibling.firstChild;
      assertEquals(RoleType.STATIC_TEXT, testNode.role);
      assertEquals('test', testNode.name);

      const ofSelectionNode = root.lastChild.lastChild;
      const cur = new Cursor(ofSelectionNode, 0);
      assertEquals('of selection', cur.selectionNode.name);
      assertEquals(RoleType.STATIC_TEXT, cur.selectionNode.role);
      assertEquals(0, cur.selectionIndex);

      const curIntoO = new Cursor(ofSelectionNode, 1);
      assertEquals('of selection', curIntoO.selectionNode.name);
      assertEquals(RoleType.STATIC_TEXT, curIntoO.selectionNode.role);
      assertEquals(1, curIntoO.selectionIndex);

      const oRange = new CursorRange(cur, curIntoO);
      oRange.select();
    });

AX_TEST_F(
    'AccessibilityExtensionCursorsTest', 'ContentEquality', async function() {
      const root = await this.runWithLoadedTree(`
    <div role="region" aria-label="test region">this is a test</button>
  `);
      const region = root.firstChild;
      assertEquals(RoleType.REGION, region.role);
      const staticText = region.firstChild;
      assertEquals(RoleType.STATIC_TEXT, staticText.role);
      const inlineTextBox = staticText.firstChild;
      assertEquals(RoleType.INLINE_TEXT_BOX, inlineTextBox.role);

      const rootRange = CursorRange.fromNode(root);
      const regionRange = CursorRange.fromNode(region);
      const staticTextRange = CursorRange.fromNode(staticText);
      const inlineTextBoxRange = CursorRange.fromNode(inlineTextBox);

      // Positive cases.
      assertTrue(regionRange.contentEquals(staticTextRange));
      assertTrue(staticTextRange.contentEquals(regionRange));
      assertTrue(inlineTextBoxRange.contentEquals(staticTextRange));
      assertTrue(staticTextRange.contentEquals(inlineTextBoxRange));

      // Negative cases.
      assertFalse(rootRange.contentEquals(regionRange));
      assertFalse(rootRange.contentEquals(staticTextRange));
      assertFalse(rootRange.contentEquals(inlineTextBoxRange));
      assertFalse(regionRange.contentEquals(rootRange));
      assertFalse(staticTextRange.contentEquals(rootRange));
      assertFalse(inlineTextBoxRange.contentEquals(rootRange));
    });

AX_TEST_F(
    'AccessibilityExtensionCursorsTest', 'DeepEquivalency', async function() {
      const root = await this.runWithLoadedTree(`
    <p style="word-spacing:100000px">this is a test</p>
  `);
      const textNode = root.find({role: RoleType.STATIC_TEXT});

      let text = new Cursor(textNode, 2);
      deep = text.deepEquivalent;
      assertEquals('this ', deep.node.name);
      assertEquals(RoleType.INLINE_TEXT_BOX, deep.node.role);
      assertEquals(2, deep.index);

      text = new Cursor(textNode, 5);
      deep = text.deepEquivalent;
      assertEquals('is ', deep.node.name);
      assertEquals(RoleType.INLINE_TEXT_BOX, deep.node.role);
      assertEquals(0, deep.index);

      text = new Cursor(textNode, 7);
      deep = text.deepEquivalent;
      assertEquals('is ', deep.node.name);
      assertEquals(RoleType.INLINE_TEXT_BOX, deep.node.role);
      assertEquals(2, deep.index);

      text = new Cursor(textNode, 8);
      deep = text.deepEquivalent;
      assertEquals('a ', deep.node.name);
      assertEquals(RoleType.INLINE_TEXT_BOX, deep.node.role);
      assertEquals(0, deep.index);

      text = new Cursor(textNode, 11);
      deep = text.deepEquivalent;
      assertEquals('test', deep.node.name);
      assertEquals(RoleType.INLINE_TEXT_BOX, deep.node.role);
      assertEquals(1, deep.index);

      // This is the only selection that can be placed at the length of the
      // node's text. This only happens at the end of a line.
      text = new Cursor(textNode, 14);
      deep = text.deepEquivalent;
      assertEquals('test', deep.node.name);
      assertEquals(RoleType.INLINE_TEXT_BOX, deep.node.role);
      assertEquals(4, deep.index);

      // However, any offset larger is invalid.
      text = new Cursor(textNode, 15);
      deep = text.deepEquivalent;
      assertTrue(text.equals(deep));
    });

AX_TEST_F(
    'AccessibilityExtensionCursorsTest', 'DeepEquivalencyBeyondLastChild',
    async function() {
      const root = await this.runWithLoadedTree(`
    <p>test</p>
  `);
      const paragraph = root.find({role: RoleType.PARAGRAPH});
      assertEquals(1, paragraph.children.length);
      const cursor = new Cursor(paragraph, 1);

      const deep = cursor.deepEquivalent;
      assertEquals(RoleType.STATIC_TEXT, deep.node.role);
      assertEquals(4, deep.index);
    });

AX_TEST_F(
    'AccessibilityExtensionCursorsTest', 'MovementByWordThroughNonInlineText',
    async function() {
      await this.runCursorMovesOnDocument(this.buttonAndInlineTextDoc, [
        // Move forward by word.
        // 'text' start and end indices.
        [WORD, DIRECTIONAL, FORWARD, {index: 7, value: 'Inline text content'}],
        [WORD, BOUND, FORWARD, {index: 11, value: 'Inline text content'}],
        // 'content' start and end incies.
        [WORD, DIRECTIONAL, FORWARD, {index: 12, value: 'Inline text content'}],
        [WORD, BOUND, FORWARD, {index: 19, value: 'Inline text content'}],
        // 'Button' start and end indices.
        [
          WORD,
          DIRECTIONAL,
          FORWARD,
          {index: 0, value: 'Button example content'},
        ],
        [WORD, BOUND, FORWARD, {index: 6, value: 'Button example content'}],
        // 'example' start and end indices.
        [
          WORD,
          DIRECTIONAL,
          FORWARD,
          {index: 7, value: 'Button example content'},
        ],
        [WORD, BOUND, FORWARD, {index: 14, value: 'Button example content'}],
        // 'content' start index. Reached last word of last object.
        [
          WORD,
          DIRECTIONAL,
          FORWARD,
          {index: 15, value: 'Button example content'},
        ],
        [
          WORD,
          DIRECTIONAL,
          FORWARD,
          {index: 15, value: 'Button example content'},
        ],

        // Move backward by word.
        // Only test start indices.
        [
          WORD,
          DIRECTIONAL,
          BACKWARD,
          {index: 7, value: 'Button example content'},
        ],
        [
          WORD,
          DIRECTIONAL,
          BACKWARD,
          {index: 0, value: 'Button example content'},
        ],
        [
          WORD,
          DIRECTIONAL,
          BACKWARD,
          {index: 12, value: 'Inline text content'},
        ],
        [WORD, DIRECTIONAL, BACKWARD, {index: 7, value: 'Inline text content'}],
        [WORD, DIRECTIONAL, BACKWARD, {index: 0, value: 'Inline text content'}],
        // Reached first word of first object.
        [WORD, DIRECTIONAL, BACKWARD, {index: 0, value: 'Inline text content'}],
      ]);
    });

AX_TEST_F(
    'AccessibilityExtensionCursorsTest', 'MovementByNodeInPdf',
    async function() {
      const root =
          createMockNode({role: chrome.automation.RoleType.ROOT_WEB_AREA});
      const pdfRoot = createMockNode(
          {role: chrome.automation.RoleType.PDF_ROOT, parent: root, root});
      const paragraph1 = createMockNode({
        role: chrome.automation.RoleType.PARAGRAPH,
        display: 'block',
        parent: pdfRoot,
        pdfRoot,
      });
      const text1 = createMockNode({
        role: chrome.automation.RoleType.STATIC_TEXT,
        parent: paragraph1,
        pdfRoot,
        name: 'First text in PDF',
      });
      const paragraph2 = createMockNode({
        role: chrome.automation.RoleType.PARAGRAPH,
        display: 'block',
        parent: pdfRoot,
        pdfRoot,
      });
      const text2 = createMockNode({
        role: chrome.automation.RoleType.STATIC_TEXT,
        parent: paragraph2,
        pdfRoot,
        name: 'Second text in PDF',
      });

      let cursor = new Cursor(root.firstChild, 0);
      assertEquals(chrome.automation.RoleType.PDF_ROOT, cursor.node.role);

      cursor = cursor.move(NODE, DIRECTIONAL, FORWARD);
      assertEquals(chrome.automation.RoleType.STATIC_TEXT, cursor.node.role);
      assertEquals('First text in PDF', cursor.node.name);

      cursor = cursor.move(NODE, DIRECTIONAL, FORWARD);
      assertEquals(chrome.automation.RoleType.STATIC_TEXT, cursor.node.role);
      assertEquals('Second text in PDF', cursor.node.name);
    });

TEST_F('AccessibilityExtensionCursorsTest', 'CopiedSelection', function() {
  const site = `
    <p>hello</p><p>world</p>
  `;
  this.runWithLoadedTree(site, async function(root) {
    const [hello, world] = root.findAll({role: RoleType.STATIC_TEXT});
    assertEquals('hello', hello.name);
    assertEquals('world', world.name);

    const range =
        new CursorRange(Cursor.fromNode(hello), Cursor.fromNode(world));
    range.select();

    // Wait for the selection to change.
    await new Promise(r => {
      root.addEventListener(EventType.DOCUMENT_SELECTION_CHANGED, r);
    });

    assertEquals(hello, root.anchorObject);
    assertEquals(0, root.anchorOffset);
    assertEquals(world, root.focusObject);
    assertEquals(5, root.focusOffset);
  });
});
