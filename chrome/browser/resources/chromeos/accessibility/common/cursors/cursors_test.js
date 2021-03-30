// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '//chrome/browser/resources/chromeos/accessibility/chromevox/testing/chromevox_next_e2e_test_base.js'
]);

/**
 * Test fixture for cursors.
 */
AccessibilityExtensionCursorsTest = class extends ChromeVoxNextE2ETest {
  /** Test cursors.Cursor. @const {string} */
  get CURSOR() {
    return 'cursor';
  }

  /** Test cursors.Range. @const {string} */
  get RANGE() {
    return 'range';
  }

  /** @override */
  setUp() {
    super.setUp();
    // Various aliases.
    window.BACKWARD = constants.Dir.BACKWARD;
    window.FORWARD = constants.Dir.FORWARD;
    window.CHARACTER = cursors.Unit.CHARACTER;
    window.WORD = cursors.Unit.WORD;
    window.LINE = cursors.Unit.LINE;
    window.NODE = cursors.Unit.NODE;
    window.BOUND = cursors.Movement.BOUND;
    window.DIRECTIONAL = cursors.Movement.DIRECTIONAL;
  }

  /**
   * Performs a series of operations on a cursor and asserts the result.
   * @param {cursors.Cursor} cursor The starting cursor.
   * @param {!Array<Array<
   *     cursors.Unit|
   *     cursors.Movement|
   *     constants.Dir|
   *     Object>>}
   *     moves An array of arrays. Each inner array contains 4 items: unit,
   *     movement, direction, and assertions object. See example below.
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
   * @param {cursors.Range} range The starting range.
   * @param {!Array<Array<
   *          cursors.Unit|
   *          cursors.Movement|
   *          constants.Dir|
   *          Object>>}
   *     moves An array of arrays. Each inner array contains 4 items: unit,
   *     direction, start and end assertions objects. See example below.
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
   * @param {Object} expected
   * @param {Cursor} cursor
   */
  makeCursorAssertion(expected, cursor) {
    if (goog.isDef(expected.value)) {
      assertEquals(expected.value, cursor.node.name);
    }
    if (goog.isDef(expected.index)) {
      assertEquals(expected.index, cursor.index);
    }
  }

  /**
   * Runs the specified moves on the |doc| and asserts expectations.
   * @param {function} doc
   * @param {string=} opt_testType Either CURSOR or RANGE.
   */
  runCursorMovesOnDocument(doc, moves, opt_testType) {
    this.runWithLoadedTree(doc, function(root) {
      let start = null;

      // This occurs as a result of a load complete.
      start =
          AutomationUtil.findNodePost(root, FORWARD, AutomationPredicate.leaf);

      const cursor = new cursors.Cursor(start, 0);
      if (!opt_testType || opt_testType === this.CURSOR) {
        const cursor = new cursors.Cursor(start, 0);
        this.cursorMoveAndAssert(cursor, moves);
      } else if (opt_testType === this.RANGE) {
        const range = new cursors.Range(cursor, cursor);
        this.rangeMoveAndAssert(range, moves);
      }
    });
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


TEST_F('AccessibilityExtensionCursorsTest', 'CharacterCursor', function() {
  this.runCursorMovesOnDocument(this.simpleDoc, [
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

TEST_F('AccessibilityExtensionCursorsTest', 'WordCursor', function() {
  this.runCursorMovesOnDocument(this.simpleDoc, [
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
    [WORD, DIRECTIONAL, BACKWARD, {index: 0, value: undefined}]
  ]);
});

TEST_F('AccessibilityExtensionCursorsTest', 'CharacterWordCursor', function() {
  this.runCursorMovesOnDocument(this.simpleDoc, [
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
    [WORD, DIRECTIONAL, BACKWARD, {index: 0, value: 'start '}]
  ]);
});

TEST_F('AccessibilityExtensionCursorsTest', 'LineCursor', function() {
  this.runCursorMovesOnDocument(this.simpleDoc, [
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
    [LINE, BOUND, BACKWARD, {value: 'start '}]
  ]);
});

TEST_F('AccessibilityExtensionCursorsTest', 'CharacterRange', function() {
  this.runCursorMovesOnDocument(
      this.simpleDoc,
      [
        [
          CHARACTER, FORWARD, {value: 'start ', index: 1},
          {value: 'start ', index: 2}
        ],
        [
          CHARACTER, FORWARD, {value: 'start ', index: 2},
          {value: 'start ', index: 3}
        ],
        [
          CHARACTER, FORWARD, {value: 'start ', index: 3},
          {value: 'start ', index: 4}
        ],
        [
          CHARACTER, FORWARD, {value: 'start ', index: 4},
          {value: 'start ', index: 5}
        ],
        [
          CHARACTER, FORWARD, {value: 'start ', index: 5},
          {value: 'start ', index: 6}
        ],

        [
          CHARACTER, FORWARD, {value: 'same line', index: 0},
          {value: 'same line', index: 1}
        ],

        [
          CHARACTER, BACKWARD, {value: 'start ', index: 5},
          {value: 'start ', index: 6}
        ],
        [
          CHARACTER, BACKWARD, {value: 'start ', index: 4},
          {value: 'start ', index: 5}
        ],
        [
          CHARACTER, BACKWARD, {value: 'start ', index: 3},
          {value: 'start ', index: 4}
        ],
        [
          CHARACTER, BACKWARD, {value: 'start ', index: 2},
          {value: 'start ', index: 3}
        ],
        [
          CHARACTER, BACKWARD, {value: 'start ', index: 1},
          {value: 'start ', index: 2}
        ],
        [
          CHARACTER, BACKWARD, {value: 'start ', index: 0},
          {value: 'start ', index: 1}
        ],
        [
          CHARACTER, BACKWARD, {value: undefined, index: 0},
          {value: undefined, index: 1}
        ],
      ],
      this.RANGE);
});

TEST_F('AccessibilityExtensionCursorsTest', 'WordRange', function() {
  this.runCursorMovesOnDocument(
      this.simpleDoc,
      [
        [
          WORD, FORWARD, {value: 'same line', index: 0},
          {value: 'same line', index: 4}
        ],
        [
          WORD, FORWARD, {value: 'same line', index: 5},
          {value: 'same line', index: 9}
        ],

        [WORD, FORWARD, {value: 'end', index: 0}, {value: 'end', index: 3}],
        [WORD, FORWARD, {value: 'end', index: 0}, {value: 'end', index: 3}],

        [
          WORD, BACKWARD, {value: 'same line', index: 5},
          {value: 'same line', index: 9}
        ],
        [
          WORD, BACKWARD, {value: 'same line', index: 0},
          {value: 'same line', index: 4}
        ],

        [
          WORD, BACKWARD, {value: 'start ', index: 0},
          {value: 'start ', index: 5}
        ],
        [
          WORD, BACKWARD, {value: undefined, index: 0},
          {value: undefined, index: 5}
        ],
      ],
      this.RANGE);
});


TEST_F('AccessibilityExtensionCursorsTest', 'LineRange', function() {
  this.runCursorMovesOnDocument(
      this.simpleDoc,
      [
        [LINE, FORWARD, {value: 'end', index: 0}, {value: 'end', index: 3}],
        [LINE, FORWARD, {value: 'end', index: 0}, {value: 'end', index: 3}],
        [LINE, FORWARD, {value: 'end', index: 0}, {value: 'end', index: 3}],
        [LINE, FORWARD, {value: 'end', index: 0}, {value: 'end', index: 3}],

        [
          LINE, BACKWARD, {value: 'start ', index: 0},
          {value: 'same line', index: 9}
        ],

        [
          LINE, BACKWARD, {value: 'start ', index: 0},
          {value: 'same line', index: 9}
        ]
      ],
      this.RANGE);
});

TEST_F(
    'AccessibilityExtensionCursorsTest', 'DontSplitOnNodeNavigation',
    function() {
      this.runWithLoadedTree(this.multiInlineDoc, function(root) {
        const para = root.firstChild;
        assertEquals('paragraph', para.role);
        let cursor = new cursors.Cursor(para.firstChild, 0);
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
    });

TEST_F('AccessibilityExtensionCursorsTest', 'WrappingCursors', function() {
  this.runWithLoadedTree(this.multiInlineDoc, function(root) {
    const first = root;
    const last = root.lastChild.firstChild;
    let cursor = new cursors.WrappingCursor(first, -1);

    // Wrap from first node to last node.
    cursor = cursor.move(NODE, DIRECTIONAL, BACKWARD);
    assertEquals(last, cursor.node);

    // Wrap from last node to first node.
    cursor = cursor.move(NODE, DIRECTIONAL, FORWARD);
    assertEquals(first, cursor.node);
  });
});

TEST_F('AccessibilityExtensionCursorsTest', 'IsInWebRange', function() {
  this.runWithLoadedTree(this.simpleDoc, function(root) {
    const para = root.firstChild;
    const webRange = cursors.Range.fromNode(para);
    const auraRange = cursors.Range.fromNode(root.parent);
    assertFalse(auraRange.isWebRange());
    assertTrue(webRange.isWebRange());
  });
});

TEST_F('AccessibilityExtensionCursorsTest', 'SingleDocSelection', function() {
  this.runWithLoadedTree(
      `
    <span>start</span>
    <p><a href="google.com">google home page</a></p>
    <p>some more text</p>
    <p>end of text</p>
  `,
      function(root) {
        // For some reason, Blink fails if we don't first select something on
        // the page.
        ChromeVoxState.instance.currentRange.select();
        const link = root.find({role: RoleType.LINK});
        const p1 = root.find({role: RoleType.PARAGRAPH});
        const p2 = p1.nextSibling;

        const singleSel = new cursors.Range(
            new cursors.Cursor(link, 0), new cursors.Cursor(link, 1));

        const multiSel = new cursors.Range(
            new cursors.Cursor(p1.firstChild, 2),
            new cursors.Cursor(p2.firstChild, 4));

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
});

TEST_F(
    'AccessibilityExtensionCursorsTest', 'MultiLineOffsetSelection',
    function() {
      this.runWithLoadedTree(this.multiInlineDoc, function(root) {
        const secondLine = root.firstChild.firstChild.firstChild.nextSibling;
        assertEquals('inlineTextBox', secondLine.role);
        assertEquals('diff ', secondLine.name);

        let secondLineCursor = new cursors.Cursor(secondLine, -1);
        // The selected node moves to the static text node.
        assertEquals(
            secondLineCursor.node.parent, secondLineCursor.selectionNode);

        // This selects the entire node via a character offset.
        assertEquals(6, secondLineCursor.selectionIndex);

        // Index into the characters.
        secondLineCursor = new cursors.Cursor(secondLine, 1);
        assertEquals(7, secondLineCursor.selectionIndex);

        // Now, try selecting via node offsets.
        let cursor = new cursors.Cursor(root.firstChild, -1);
        assertEquals(root, cursor.selectionNode);
        assertEquals(0, cursor.selectionIndex);

        cursor = new cursors.Cursor(root.firstChild.nextSibling, -1);
        assertEquals(root, cursor.selectionNode);
        assertEquals(1, cursor.selectionIndex);
      });
    });

TEST_F('AccessibilityExtensionCursorsTest', 'InlineElementOffset', function() {
  this.runWithLoadedTree(
      `
    <span>start</span>
    <p>This<br> is a<a href="#g">test</a>of selection</p>
  `,
      function(root) {
        root.addEventListener(
            'textSelectionChanged', this.newCallback(function(evt) {
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
        const cur = new cursors.Cursor(ofSelectionNode, 0);
        assertEquals('of selection', cur.selectionNode.name);
        assertEquals(RoleType.STATIC_TEXT, cur.selectionNode.role);
        assertEquals(0, cur.selectionIndex);

        const curIntoO = new cursors.Cursor(ofSelectionNode, 1);
        assertEquals('of selection', curIntoO.selectionNode.name);
        assertEquals(RoleType.STATIC_TEXT, curIntoO.selectionNode.role);
        assertEquals(1, curIntoO.selectionIndex);

        const oRange = new cursors.Range(cur, curIntoO);
        oRange.select();
      });
});

TEST_F('AccessibilityExtensionCursorsTest', 'ContentEquality', function() {
  this.runWithLoadedTree(
      `
    <div role="region">this is a test</button>
  `,
      function(root) {
        const region = root.firstChild;
        assertEquals(RoleType.REGION, region.role);
        const staticText = region.firstChild;
        assertEquals(RoleType.STATIC_TEXT, staticText.role);
        const inlineTextBox = staticText.firstChild;
        assertEquals(RoleType.INLINE_TEXT_BOX, inlineTextBox.role);

        const rootRange = cursors.Range.fromNode(root);
        const regionRange = cursors.Range.fromNode(region);
        const staticTextRange = cursors.Range.fromNode(staticText);
        const inlineTextBoxRange = cursors.Range.fromNode(inlineTextBox);

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
});

TEST_F('AccessibilityExtensionCursorsTest', 'DeepEquivalency', function() {
  this.runWithLoadedTree(
      `
    <p style="word-spacing:100000px">this is a test</p>
  `,
      function(root) {
        const textNode = root.find({role: RoleType.STATIC_TEXT});

        let text = new cursors.Cursor(textNode, 2);
        deep = text.deepEquivalent;
        assertEquals('this ', deep.node.name);
        assertEquals(RoleType.INLINE_TEXT_BOX, deep.node.role);
        assertEquals(2, deep.index);

        text = new cursors.Cursor(textNode, 5);
        deep = text.deepEquivalent;
        assertEquals('is ', deep.node.name);
        assertEquals(RoleType.INLINE_TEXT_BOX, deep.node.role);
        assertEquals(0, deep.index);

        text = new cursors.Cursor(textNode, 7);
        deep = text.deepEquivalent;
        assertEquals('is ', deep.node.name);
        assertEquals(RoleType.INLINE_TEXT_BOX, deep.node.role);
        assertEquals(2, deep.index);

        text = new cursors.Cursor(textNode, 8);
        deep = text.deepEquivalent;
        assertEquals('a ', deep.node.name);
        assertEquals(RoleType.INLINE_TEXT_BOX, deep.node.role);
        assertEquals(0, deep.index);

        text = new cursors.Cursor(textNode, 11);
        deep = text.deepEquivalent;
        assertEquals('test', deep.node.name);
        assertEquals(RoleType.INLINE_TEXT_BOX, deep.node.role);
        assertEquals(1, deep.index);

        // This is the only selection that can be placed at the length of the
        // node's text. This only happens at the end of a line.
        text = new cursors.Cursor(textNode, 14);
        deep = text.deepEquivalent;
        assertEquals('test', deep.node.name);
        assertEquals(RoleType.INLINE_TEXT_BOX, deep.node.role);
        assertEquals(4, deep.index);

        // However, any offset larger is invalid.
        text = new cursors.Cursor(textNode, 15);
        deep = text.deepEquivalent;
        assertTrue(text.equals(deep));
      });
});

TEST_F(
    'AccessibilityExtensionCursorsTest', 'DeepEquivalencyBeyondLastChild',
    function() {
      this.runWithLoadedTree(
          `
    <p>test</p>
  `,
          function(root) {
            const paragraph = root.find({role: RoleType.PARAGRAPH});
            assertEquals(1, paragraph.children.length);
            const cursor = new cursors.Cursor(paragraph, 1);

            const deep = cursor.deepEquivalent;
            assertEquals(RoleType.STATIC_TEXT, deep.node.role);
            assertEquals(4, deep.index);
          });
    });

TEST_F(
    'AccessibilityExtensionCursorsTest', 'SelectionAdjustmentsRichText',
    function() {
      this.runWithLoadedTree(
          `
    <div contenteditable><p>test</p><p>123</p></div>
  `,
          function(root) {
            const textField = root.firstChild;
            const paragraph = textField.firstChild;
            const otherParagraph = textField.lastChild;
            const staticText = paragraph.firstChild;
            const otherStaticText = otherParagraph.firstChild;

            // Ranges by default surround a node. Ensure it results in a
            // collapsed selection.
            let range = cursors.Range.fromNode(staticText);
            assertEquals(0, range.start.selectionIndex);
            assertEquals(0, range.end.selectionIndex);
            assertEquals(paragraph, range.start.selectionNode);
            assertEquals(paragraph, range.end.selectionNode);

            // Text selection.
            range = new cursors.Range(
                new cursors.Cursor(staticText, 2),
                new cursors.Cursor(staticText, 4));
            assertEquals(2, range.start.selectionIndex);
            assertEquals(4, range.end.selectionIndex);
            assertEquals(staticText, range.start.selectionNode);
            assertEquals(staticText, range.end.selectionNode);

            // Tree selection.
            range = cursors.Range.fromNode(paragraph);
            assertEquals(0, range.start.selectionIndex);
            assertEquals(0, range.end.selectionIndex);
            assertEquals(textField, range.start.selectionNode);
            assertEquals(textField, range.end.selectionNode);

            range = cursors.Range.fromNode(otherStaticText);
            assertEquals(0, range.start.selectionIndex);
            assertEquals(0, range.end.selectionIndex);
            assertEquals(otherParagraph, range.start.selectionNode);
            assertEquals(otherParagraph, range.end.selectionNode);

            range = cursors.Range.fromNode(otherParagraph);
            assertEquals(1, range.start.selectionIndex);
            assertEquals(1, range.end.selectionIndex);
            assertEquals(textField, range.start.selectionNode);
            assertEquals(textField, range.end.selectionNode);
          });
    });

TEST_F(
    'AccessibilityExtensionCursorsTest', 'SelectionAdjustmentsNonRichText',
    function() {
      this.runWithLoadedTree(
          `
    <input type="text"></input>
    <textarea></textarea>
  `,
          function(root) {
            const testEditable = function(edit) {
              // Occurs as part of ordinary (non-text) navigation.
              let range = cursors.Range.fromNode(edit);
              assertEquals(-1, range.start.selectionIndex);
              assertEquals(-1, range.end.selectionIndex);
              assertEquals(edit, range.start.selectionNode);
              assertEquals(edit, range.end.selectionNode);

              // Occurs as a result of explicit text nav e.g. nextCharacter
              // command.
              range = new cursors.Range(
                  new cursors.Cursor(edit, 2), new cursors.Cursor(edit, 3));
              assertEquals(2, range.start.selectionIndex);
              assertEquals(3, range.end.selectionIndex);
              assertEquals(edit, range.start.selectionNode);
              assertEquals(edit, range.end.selectionNode);
            };

            const textField = root.firstChild.firstChild;
            const textArea = root.lastChild.lastChild;

            // Both of these should behave in the same way.
            testEditable(textField);
            testEditable(textArea);
          });
    });

TEST_F(
    'AccessibilityExtensionCursorsTest', 'MovementByWordThroughNonInlineText',
    function() {
      this.runCursorMovesOnDocument(this.buttonAndInlineTextDoc, [
        // Move forward by word.
        // 'text' start and end indices.
        [WORD, DIRECTIONAL, FORWARD, {index: 7, value: 'Inline text content'}],
        [WORD, BOUND, FORWARD, {index: 11, value: 'Inline text content'}],
        // 'content' start and end incies.
        [WORD, DIRECTIONAL, FORWARD, {index: 12, value: 'Inline text content'}],
        [WORD, BOUND, FORWARD, {index: 19, value: 'Inline text content'}],
        // 'Button' start and end indices.
        [
          WORD, DIRECTIONAL, FORWARD,
          {index: 0, value: 'Button example content'}
        ],
        [WORD, BOUND, FORWARD, {index: 6, value: 'Button example content'}],
        // 'example' start and end indices.
        [
          WORD, DIRECTIONAL, FORWARD,
          {index: 7, value: 'Button example content'}
        ],
        [WORD, BOUND, FORWARD, {index: 14, value: 'Button example content'}],
        // 'content' start index. Reached last word of last object.
        [
          WORD, DIRECTIONAL, FORWARD,
          {index: 15, value: 'Button example content'}
        ],
        [
          WORD, DIRECTIONAL, FORWARD,
          {index: 15, value: 'Button example content'}
        ],

        // Move backward by word.
        // Only test start indices.
        [
          WORD, DIRECTIONAL, BACKWARD,
          {index: 7, value: 'Button example content'}
        ],
        [
          WORD, DIRECTIONAL, BACKWARD,
          {index: 0, value: 'Button example content'}
        ],
        [
          WORD, DIRECTIONAL, BACKWARD, {index: 12, value: 'Inline text content'}
        ],
        [WORD, DIRECTIONAL, BACKWARD, {index: 7, value: 'Inline text content'}],
        [WORD, DIRECTIONAL, BACKWARD, {index: 0, value: 'Inline text content'}],
        // Reached first word of first object.
        [WORD, DIRECTIONAL, BACKWARD, {index: 0, value: 'Inline text content'}]
      ]);
    });
