// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['switch_access_e2e_test_base.js']);

/** Text fixture for the text navigation manager. */
SwitchAccessTextNavigationManagerTest = class extends SwitchAccessE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    this.textNavigationManager = TextNavigationManager.instance;
    this.navigationManager = Navigator.byItem;
  }
};


/**
 * Generates a website with a text area, finds the node for the text
 * area, sets up the node to listen for a text navigation action, and then
 * executes the specified text navigation action. Upon detecting the
 * text navigation action, the node will verify that the action correctly
 * changed the index of the text caret.
 * @param {!SwitchAccessE2ETest} testFixture
 * @param {{content: string,
 *          initialIndex: number,
 *          targetIndex: number,
 *          navigationAction: function(),
 *          id: (string || undefined),
 *          cols: (number || undefined),
 *          wrap: (string || undefined)}} textParams
 */
async function runTextNavigationTest(testFixture, textParams) {
  // Required parameters.
  const textContent = textParams.content;
  const initialTextIndex = textParams.initialIndex;
  const targetTextIndex = textParams.targetIndex;
  const textNavigationAction = textParams.navigationAction;

  // Default parameters.
  const textId = textParams.id || 'test';
  const textCols = textParams.cols || 20;
  const textWrap = textParams.wrap || 'soft';

  const website = generateWebsiteWithTextArea(
      textId, textContent, initialTextIndex, textCols, textWrap);

  await testFixture.runWithLoadedTree(website);
  const inputNode = this.findNodeById(textId);
  assertNotEquals(inputNode, null);

  setUpCursorChangeListener(
      testFixture, inputNode, initialTextIndex, targetTextIndex,
      targetTextIndex);

  textNavigationAction();
}

/**
 * This function:
 * - Generates a website with a text area
 * - Executes setSelectStart finds the node for the text
 * area
 * - Sets up the node to listen for a text navigation action
 * - executes the specified text navigation action. Upon detecting the
 * - Verifies that the action correctly changed the index of the text caret
 * - Sets up a second listener for a text selection action
 * - Calls saveSelectEnd function from the event listener
 * - Verifies that the selection was set correctly
 * textParams should specify parameters
 * for the test as follows:
 *  -content: content of the text area.
 *  -initialIndex: index of the text caret before the navigation action.
 *  -targetStartIndex: start index of the selection after the selection action.
 *  -targetEndIndex: end index of the selection after the navigation action.
 *  -navigationAction: function executing a text navigation action or selection
 * action. -id: id of the text area element (optional). -cols: number of columns
 * in the text area (optional). -wrap: the wrap attribute ("hard" or "soft") of
 * the text area (optional).
 *
 * @param {!SwitchAccessE2ETest} testFixture
 * @param {selectionTextParams} textParams,
 */
async function runTextSelectionTest(testFixture, textParams) {
  // Required parameters.
  const textContent = textParams.content;
  const initialTextIndex = textParams.initialIndex;
  const targetTextStartIndex = textParams.targetStartIndex;
  const targetTextEndIndex = textParams.targetEndIndex;
  const textNavigationAction = textParams.navigationAction;

  // Default parameters.
  const selectionIsBackward = textParams.backward || false;
  const textId = textParams.id || 'test';
  const textCols = textParams.cols || 20;
  const textWrap = textParams.wrap || 'soft';

  const website = generateWebsiteWithTextArea(
      textId, textContent, initialTextIndex, textCols, textWrap);

  let navigationTargetIndex = targetTextEndIndex;
  if (selectionIsBackward) {
    navigationTargetIndex = targetTextStartIndex;
  }

  await testFixture.runWithLoadedTree(website);
  const inputNode = this.findNodeById(textId);
  assertNotEquals(inputNode, null);
  checkNodeIsFocused(inputNode);
  const callback = testFixture.newCallback(function() {
    setUpCursorChangeListener(
        testFixture, inputNode, targetTextEndIndex, targetTextStartIndex,
        targetTextEndIndex);
    testFixture.textNavigationManager.saveSelectEnd();
  });

  testFixture.textNavigationManager.saveSelectStart();

  setUpCursorChangeListener(
      testFixture, inputNode, initialTextIndex, navigationTargetIndex,
      navigationTargetIndex, callback);

  textNavigationAction();
}

/**
 * Returns a website string with a text area with the given properties.
 * @param {number} id The ID of the text area element.
 * @param {string} contents The contents of the text area.
 * @param {number} textIndex The index of the text caret within the text area.
 * @param {number} cols The number of columns in the text area.
 * @param {string} wrap The wrap attribute of the text area ('hard' or 'soft').
 * @return {string}
 */
function generateWebsiteWithTextArea(id, contents, textIndex, cols, wrap) {
  const website = `data:text/html;charset=utf-8,
    <textarea id=${id} cols=${cols} wrap=${wrap}
    autofocus="true">${contents}</textarea>
    <script>
      const input = document.getElementById("${id}");
      input.selectionStart = ${textIndex};
      input.selectionEnd = ${textIndex};
      input.focus();
    </script>`;
  return website;
}

/**
 * Check that the node in the JS file matches the node in the test.
 * The nodes can be assumed to be the same if their roles match as there is only
 * one text input node on the generated webpage.
 * @param {!AutomationNode} inputNode
 */
function checkNodeIsFocused(inputNode) {
  chrome.automation.getFocus(focusedNode => {
    assertEquals(focusedNode.role, inputNode.role);
  });
}

/**
 * Sets up the input node (text field) to listen for text
 * navigation and selection actions. When the index of the text selection
 * changes from its initial position, checks that the text
 * indices now matches the target text start and end index. Assumes that the
 * initial and target text start/end indices are distinct (to detect a
 * change from the text navigation action). Also assumes that
 * the text navigation and selection actions directly changes the text caret
 * to the correct index (with no intermediate movements).
 * @param {!SwitchAccessE2ETest} testFixture
 * @param {!AutomationNode} inputNode
 * @param {number} initialTextIndex
 * @param {number} targetTextStartIndex
 * @param {number} targetTextEndIndex
 * @param {function() || undefined} callback
 */
function setUpCursorChangeListener(
    testFixture, inputNode, initialTextIndex, targetTextStartIndex,
    targetTextEndIndex, callback) {
  // Ensures that the text index has changed before checking the new index.
  const checkActionFinished = function(tab) {
    if (inputNode.textSelStart !== initialTextIndex ||
        inputNode.textSelEnd !== initialTextIndex) {
      checkTextIndex();
      if (callback) {
        callback();
      }
    }
  };

  // Test will not exit until this check is called.
  const checkTextIndex = testFixture.newCallback(function() {
    assertEquals(inputNode.textSelStart, targetTextStartIndex);
    assertEquals(inputNode.textSelEnd, targetTextEndIndex);
    // If there's a callback then this is the navigation listener for a
    // selection test, thus remove it when fired to make way for the selection
    // listener.
    if (callback) {
      inputNode.removeEventListener(
          chrome.automation.EventType.TEXT_SELECTION_CHANGED,
          checkActionFinished);
    }
  });

  inputNode.addEventListener(
      chrome.automation.EventType.TEXT_SELECTION_CHANGED, checkActionFinished);
}

// TODO(crbug.com/1268230): Re-enable test.
AX_TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_JumpToBeginning',
    async function() {
      await runTextNavigationTest(this, {
        content: 'hi there',
        initialIndex: 6,
        targetIndex: 0,
        navigationAction: () => {
          TextNavigationManager.jumpToBeginning();
        },
      });
    });

// TODO(crbug.com/1268230): Re-enable test.
AX_TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_JumpToEnd',
    async function() {
      await runTextNavigationTest(this, {
        content: 'hi there',
        initialIndex: 3,
        targetIndex: 8,
        navigationAction: () => {
          TextNavigationManager.jumpToEnd();
        },
      });
    });

// TODO(crbug.com/1177096) Renable test
AX_TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_MoveBackwardOneChar',
    async function() {
      await runTextNavigationTest(this, {
        content: 'parrots!',
        initialIndex: 7,
        targetIndex: 6,
        navigationAction: () => {
          TextNavigationManager.moveBackwardOneChar();
        },
      });
    });

// TODO(crbug.com/1268230): Re-enable test.
AX_TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_MoveBackwardOneWord',
    async function() {
      await runTextNavigationTest(this, {
        content: 'more parrots!',
        initialIndex: 5,
        targetIndex: 0,
        navigationAction: () => {
          TextNavigationManager.moveBackwardOneWord();
        },
      });
    });

// TODO(crbug.com/1268230): Re-enable test.
AX_TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_MoveForwardOneChar',
    async function() {
      await runTextNavigationTest(this, {
        content: 'hello',
        initialIndex: 0,
        targetIndex: 1,
        navigationAction: () => {
          TextNavigationManager.moveForwardOneChar();
        },
      });
    });

// TODO(crbug.com/1268230): Re-enable test.
AX_TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_MoveForwardOneWord',
    async function() {
      await runTextNavigationTest(this, {
        content: 'more parrots!',
        initialIndex: 4,
        targetIndex: 12,
        navigationAction: () => {
          TextNavigationManager.moveForwardOneWord();
        },
      });
    });

// TODO(crbug.com/1268230): Re-enable test.
AX_TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_MoveUpOneLine',
    async function() {
      await runTextNavigationTest(this, {
        content: 'more parrots!',
        initialIndex: 7,
        targetIndex: 2,
        cols: 8,
        wrap: 'hard',
        navigationAction: () => {
          TextNavigationManager.moveUpOneLine();
        },
      });
    });

// TODO(crbug.com/1268230): Re-enable test.
AX_TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_MoveDownOneLine',
    async function() {
      await runTextNavigationTest(this, {
        content: 'more parrots!',
        initialIndex: 3,
        targetIndex: 8,
        cols: 8,
        wrap: 'hard',
        navigationAction: () => {
          TextNavigationManager.moveDownOneLine();
        },
      });
    });


/**
 * Test the setSelectStart function by checking correct index is stored as the
 * selection start index.
 */
AX_TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_SelectStart',
    async function() {
      const website =
          generateWebsiteWithTextArea('test', 'test123', 3, 20, 'hard');

      await this.runWithLoadedTree(website);
      const inputNode = this.findNodeById('test');
      assertNotEquals(inputNode, null);
      checkNodeIsFocused(inputNode);

      this.textNavigationManager.saveSelectStart();
      const startIndex = this.textNavigationManager.selectionStartIndex_;
      assertEquals(startIndex, 3);
    });

/**
 * Test the setSelectEnd function by manually setting the selection start index
 * and node then calling setSelectEnd and checking for the correct selection
 * bounds
 */
AX_TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_SelectEnd',
    async function() {
      const website =
          generateWebsiteWithTextArea('test', 'test 123', 6, 20, 'hard');

      await this.runWithLoadedTree(website);
      const inputNode = this.findNodeById('test');
      assertNotEquals(inputNode, null);
      checkNodeIsFocused(inputNode);


      const startIndex = 3;
      this.textNavigationManager.selectionStartIndex_ = startIndex;
      this.textNavigationManager.selectionStartObject_ = inputNode;
      this.textNavigationManager.saveSelectEnd();
      const endIndex = inputNode.textSelEnd;
      assertEquals(6, endIndex);
    });

/**
 * Test use of setSelectStart and setSelectEnd with the moveForwardOneChar
 * function.
 */
AX_TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_SelectCharacter',
    async function() {
      await runTextSelectionTest(this, {
        content: 'hello world!',
        initialIndex: 0,
        targetStartIndex: 0,
        targetEndIndex: 1,
        cols: 8,
        wrap: 'hard',
        navigationAction: () => {
          TextNavigationManager.moveForwardOneChar();
        },
      });
    });

/**
 * Test use of setSelectStart and setSelectEnd with a backward selection using
 * the moveBackwardOneWord function.
 */
AX_TEST_F(
    'SwitchAccessTextNavigationManagerTest', 'DISABLED_SelectWordBackward',
    async function() {
      await runTextSelectionTest(this, {
        content: 'hello world!',
        initialIndex: 5,
        targetStartIndex: 0,
        targetEndIndex: 5,
        cols: 8,
        wrap: 'hard',
        navigationAction: () => {
          TextNavigationManager.moveBackwardOneWord();
        },
        backward: true,
      });
    });

/**
 * selectionTextParams should specify parameters
 * for the test as follows:
 *  -content: content of the text area.
 *  -initialIndex: index of the text caret before the navigation action.
 *  -targetIndex: index of the text caret after the navigation action.
 *  -navigationAction: function executing a text navigation action.
 *  -id: id of the text area element (optional).
 *  -cols: number of columns in the text area (optional).
 *  -wrap: the wrap attribute ("hard" or "soft") of the text area (optional).
 *@typedef {{content: string,
 *          initialIndex: number,
 *          targetStartIndex: number,
 *          targetEndIndex: number,
 *          textAction: function(),
 *          backward: (boolean || undefined)
 *          id: (string || undefined),
 *          cols: (number || undefined),
 *          wrap: (string || undefined),}}
 */
let selectionTextParams;
