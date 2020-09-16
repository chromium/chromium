// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_next_e2e_test_base.js']);

/**
 * Test fixture for UserActionMonitor.
 */
ChromeVoxUserActionMonitorTest = class extends ChromeVoxNextE2ETest {
  /**
   * Create mock event object.
   * @param {number} keyCode
   * @param {{altGraphKey: boolean=,
   *         altKey: boolean=,
   *         ctrlKey: boolean=,
   *         metaKey: boolean=,
   *         searchKeyHeld: boolean=,
   *         shiftKey: boolean=,
   *         stickyMode: boolean=,
   *         prefixKey: boolean=}=} opt_modifiers
   * @return {Object} The mock event.
   */
  createMockKeyDownEvent(keyCode, opt_modifiers) {
    const modifiers = opt_modifiers === undefined ? {} : opt_modifiers;
    const mockEvent = {};
    mockEvent.keyCode = keyCode;
    for (const key in modifiers) {
      mockEvent[key] = modifiers[key];
    }

    mockEvent.preventDefault = () => {};
    mockEvent.stopPropagation = () => {};
    return mockEvent;
  }

  /**
   * Returns the start node of the current ChromeVox range.
   * @return {AutomationNode}
   */
  getRangeStart() {
    return ChromeVoxState.instance.getCurrentRange().start.node;
  }

  get simpleDoc() {
    return `
      <p>test</p>
    `;
  }

  get paragraphDoc() {
    return `
      <p>Start</p>
      <p>End</p>
    `;
  }
};

TEST_F('ChromeVoxUserActionMonitorTest', 'UnitTest', function() {
  this.runWithLoadedTree(this.simpleDoc, function() {
    let finished = false;
    const actions = [
      {
        type: 'key_sequence',
        value: {'keys': {'keyCode': [KeyCode.SPACE]}},
      },
      {type: 'braille', value: 'jumpToTop'},
      {type: 'gesture', value: 'swipeUp1'}
    ];
    const onFinished = () => finished = true;

    const monitor = new UserActionMonitor(actions, onFinished);
    assertEquals(3, monitor.actions_.length);
    assertEquals(0, monitor.actionIndex_);
    assertEquals('key_sequence', monitor.getExpectedAction_().type);
    assertFalse(finished);
    monitor.expectedActionMatched_();
    assertEquals(1, monitor.actionIndex_);
    assertEquals('braille', monitor.getExpectedAction_().type);
    assertFalse(finished);
    monitor.expectedActionMatched_();
    assertEquals(2, monitor.actionIndex_);
    assertEquals('gesture', monitor.getExpectedAction_().type);
    assertFalse(finished);
    monitor.expectedActionMatched_();
    assertTrue(finished);
    assertEquals(3, monitor.actions_.length);
    assertEquals(3, monitor.actionIndex_);
  });
});

TEST_F('ChromeVoxUserActionMonitorTest', 'ActionUnitTest', function() {
  this.runWithLoadedTree(this.simpleDoc, function() {
    const keySequenceActionOne = UserActionMonitor.Action.fromActionInfo(
        {type: 'key_sequence', value: {keys: {keyCode: [KeyCode.SPACE]}}});
    const keySequenceActionTwo = new UserActionMonitor.Action(
        'key_sequence', new KeySequence(this.createMockKeyDownEvent(65)));
    const gestureActionOne = UserActionMonitor.Action.fromActionInfo(
        {type: 'gesture', value: 'swipeUp1'});
    const gestureActionTwo =
        new UserActionMonitor.Action('gesture', 'swipeUp2');

    assertFalse(keySequenceActionOne.equals(keySequenceActionTwo));
    assertFalse(keySequenceActionOne.equals(gestureActionOne));
    assertFalse(keySequenceActionOne.equals(gestureActionTwo));
    assertFalse(keySequenceActionTwo.equals(gestureActionOne));
    assertFalse(keySequenceActionTwo.equals(gestureActionTwo));
    assertFalse(gestureActionOne.equals(gestureActionTwo));

    const cloneKeySequenceActionOne = UserActionMonitor.Action.fromActionInfo(
        {type: 'key_sequence', value: {keys: {keyCode: [KeyCode.SPACE]}}});
    const cloneGestureActionOne =
        new UserActionMonitor.Action('gesture', 'swipeUp1');
    assertTrue(keySequenceActionOne.equals(cloneKeySequenceActionOne));
    assertTrue(gestureActionOne.equals(cloneGestureActionOne));
  });
});

TEST_F('ChromeVoxUserActionMonitorTest', 'Errors', function() {
  this.runWithLoadedTree(this.simpleDoc, function() {
    let monitor;
    let caught = false;
    let finished = false;
    const actions = [
      {
        type: 'key_sequence',
        value: {'keys': {'keyCode': [KeyCode.SPACE]}},
      },
    ];
    const onFinished = () => finished = true;
    const assertCaughtAndReset = () => {
      assertTrue(caught);
      caught = false;
    };

    try {
      monitor = new UserActionMonitor([], onFinished);
      assertTrue(false);  // Shouldn't execute.
    } catch (error) {
      assertEquals(
          `UserActionMonitor: actionInfos can't be empty`, error.message);
      caught = true;
    }
    assertCaughtAndReset();
    try {
      new UserActionMonitor.Action('key_sequence', 'invalid');
      assertTrue(false);  // Shouldn't execute
    } catch (error) {
      assertEquals(
          'UserActionMonitor: Must provide a KeySequence value for Actions ' +
              'of type ActionType.KEY_SEQUENCE',
          error.message);
      caught = true;
    }
    assertCaughtAndReset();
    try {
      UserActionMonitor.Action.fromActionInfo({type: 'gesture', value: false});
      assertTrue(false);  // Shouldn't execute.
    } catch (error) {
      assertEquals(
          'UserActionMonitor: Must provide a string value for Actions if ' +
              'type is other than ActionType.KEY_SEQUENCE',
          error.message);
      caught = true;
    }
    assertCaughtAndReset();

    monitor = new UserActionMonitor(actions, onFinished);
    monitor.expectedActionMatched_();
    assertTrue(finished);

    try {
      monitor.onKeySequence(new KeySequence(this.createMockKeyDownEvent(32)));
      assertTrue(false);  // Shouldn't execute.
    } catch (error) {
      assertEquals(
          'UserActionMonitor: actionIndex_ is invalid.', error.message);
      caught = true;
    }
    assertCaughtAndReset();
    try {
      monitor.expectedActionMatched_();
      assertTrue(false);  // Shouldn't execute.
    } catch (error) {
      assertEquals(
          'UserActionMonitor: actionIndex_ is invalid.', error.message);
      caught = true;
    }
    assertCaughtAndReset();
    try {
      monitor.nextAction_();
      assertTrue(false);  // Shouldn't execute.
    } catch (error) {
      assertEquals(
          `UserActionMonitor: can't call nextAction_(), invalid index`,
          error.message);
      caught = true;
    }
    assertTrue(caught);
  });
});

TEST_F('ChromeVoxUserActionMonitorTest', 'Output', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, function(rootNode) {
    let monitor;
    let finished = false;
    const actions = [
      {
        type: 'gesture',
        value: 'swipeUp1',
        beforeActionMsg: 'First instruction',
        afterActionMsg: 'Congratulations!'
      },
      {
        type: 'gesture',
        value: 'swipeUp1',
        beforeActionMsg: 'Second instruction',
        afterActionMsg: 'You did it!'
      }
    ];
    const onFinished = () => finished = true;

    mockFeedback
        .call(() => {
          monitor = new UserActionMonitor(actions, onFinished);
        })
        .expectSpeech('First instruction')
        .call(() => {
          monitor.expectedActionMatched_();
          assertFalse(finished);
        })
        .expectSpeech('Congratulations!', 'Second instruction')
        .call(() => {
          monitor.expectedActionMatched_();
          assertTrue(finished);
        })
        .expectSpeech('You did it!');
    mockFeedback.replay();
  });
});

// Tests that we can match a single key. Serves as an integration test
// since we don't directly call a UserActionMonitor function.
TEST_F('ChromeVoxUserActionMonitorTest', 'SingleKey', function() {
  this.runWithLoadedTree(this.simpleDoc, function() {
    const keyboardHandler = new BackgroundKeyboardHandler();
    let finished = false;
    const actions =
        [{type: 'key_sequence', value: {'keys': {'keyCode': [KeyCode.SPACE]}}}];
    const onFinished = () => finished = true;

    ChromeVoxState.instance.createUserActionMonitor(actions, onFinished);
    keyboardHandler.onKeyDown(this.createMockKeyDownEvent(KeyCode.LEFT));
    keyboardHandler.onKeyUp(this.createMockKeyDownEvent(KeyCode.LEFT));
    assertFalse(finished);
    keyboardHandler.onKeyDown(this.createMockKeyDownEvent(KeyCode.RIGHT));
    keyboardHandler.onKeyUp(this.createMockKeyDownEvent(KeyCode.RIGHT));
    assertFalse(finished);
    keyboardHandler.onKeyDown(this.createMockKeyDownEvent(KeyCode.SPACE));
    keyboardHandler.onKeyUp(this.createMockKeyDownEvent(KeyCode.SPACE));
    assertTrue(finished);
  });
});

// Tests that we can match a key sequence. Serves as an integration test
// since we don't directly call a UserActionMonitor function.
TEST_F('ChromeVoxUserActionMonitorTest', 'MultipleKeys', function() {
  this.runWithLoadedTree(this.simpleDoc, function() {
    const keyboardHandler = new BackgroundKeyboardHandler();
    let finished = false;
    const actions = [{
      type: 'key_sequence',
      value: {'cvoxModifier': true, 'keys': {'keyCode': [KeyCode.O, KeyCode.B]}}
    }];
    const onFinished = () => finished = true;

    ChromeVoxState.instance.createUserActionMonitor(actions, onFinished);
    keyboardHandler.onKeyDown(this.createMockKeyDownEvent(KeyCode.O));
    keyboardHandler.onKeyUp(this.createMockKeyDownEvent(KeyCode.O));
    assertFalse(finished);
    keyboardHandler.onKeyDown(this.createMockKeyDownEvent(KeyCode.B));
    keyboardHandler.onKeyUp(this.createMockKeyDownEvent(KeyCode.B));
    assertFalse(finished);
    keyboardHandler.onKeyDown(this.createMockKeyDownEvent(KeyCode.SEARCH));
    keyboardHandler.onKeyUp(this.createMockKeyDownEvent(KeyCode.SEARCH));
    assertFalse(finished);
    keyboardHandler.onKeyDown(
        this.createMockKeyDownEvent(KeyCode.O, {searchKeyHeld: true}));
    assertFalse(finished);
    keyboardHandler.onKeyUp(
        this.createMockKeyDownEvent(KeyCode.O, {searchKeyHeld: true}));
    keyboardHandler.onKeyDown(this.createMockKeyDownEvent(KeyCode.B));
    assertTrue(finished);
  });
});

// Tests that we can match multiple key sequences.
TEST_F('ChromeVoxUserActionMonitorTest', 'MultipleKeySequences', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, function() {
    let finished = false;
    const actions = [
      {
        type: 'key_sequence',
        value: {
          'keys': {'altKey': [true], 'shiftKey': [true], 'keyCode': [KeyCode.L]}
        },
        afterActionMsg: 'You pressed the first sequence!'
      },
      {
        type: 'key_sequence',
        value: {
          'keys': {'altKey': [true], 'shiftKey': [true], 'keyCode': [KeyCode.S]}
        },
        afterActionMsg: 'You pressed the second sequence!'
      }
    ];
    const onFinished = () => finished = true;

    const altShiftLSequence = new KeySequence(
        this.createMockKeyDownEvent(76, {altKey: true, shiftKey: true}));
    const altShiftSSequence = new KeySequence(
        this.createMockKeyDownEvent(83, {altKey: true, shiftKey: true}));
    let monitor;
    mockFeedback
        .call(() => {
          monitor = new UserActionMonitor(actions, onFinished);
          assertFalse(monitor.onKeySequence(altShiftSSequence));
          assertFalse(finished);
          assertTrue(monitor.onKeySequence(altShiftLSequence));
          assertFalse(finished);
        })
        .expectSpeech('You pressed the first sequence!')
        .call(() => {
          assertFalse(monitor.onKeySequence(altShiftLSequence));
          assertFalse(finished);
          assertTrue(monitor.onKeySequence(altShiftSSequence));
          assertTrue(finished);
        })
        .expectSpeech('You pressed the second sequence!');
    mockFeedback.replay();
  });
});

// Tests that we can provide expectations for ChromeVox commands and block
// command execution until the desired command is performed. Serves as an
// integration test since we don't directly call a UserActionMonitor function.
TEST_F('ChromeVoxUserActionMonitorTest', 'BlockCommands', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.paragraphDoc, function() {
    const keyboardHandler = new BackgroundKeyboardHandler();
    let finished = false;
    const actions = [
      {
        type: 'key_sequence',
        value: {'cvoxModifier': true, 'keys': {'keyCode': [KeyCode.RIGHT]}}
      },
      {
        type: 'key_sequence',
        value: {'cvoxModifier': true, 'keys': {'keyCode': [KeyCode.LEFT]}}
      }
    ];
    const onFinished = () => finished = true;

    const nextObject =
        this.createMockKeyDownEvent(KeyCode.RIGHT, {searchKeyHeld: true});
    const nextLine =
        this.createMockKeyDownEvent(40 /* ArrowDown */, {searchKeyHeld: true});
    const previousObject =
        this.createMockKeyDownEvent(KeyCode.LEFT, {searchKeyHeld: true});
    const previousLine =
        this.createMockKeyDownEvent(38 /* ArrowUp */, {searchKeyHeld: true});

    ChromeVoxState.instance.createUserActionMonitor(actions, onFinished);
    mockFeedback.expectSpeech('Start')
        .call(() => {
          assertEquals('Start', this.getRangeStart().name);
        })
        .call(() => {
          // Calling nextLine doesn't move ChromeVox because UserActionMonitor
          // expects the nextObject command.
          keyboardHandler.onKeyDown(nextLine);
          keyboardHandler.onKeyUp(nextLine);
          assertEquals('Start', this.getRangeStart().name);
        })
        .call(() => {
          keyboardHandler.onKeyDown(nextObject);
          keyboardHandler.onKeyUp(nextObject);
          assertEquals('End', this.getRangeStart().name);
        })
        .expectSpeech('End')
        .call(() => {
          // Calling previousLine doesn't move ChromeVox because
          // UserActionMonitor expects the previousObject command.
          keyboardHandler.onKeyDown(previousLine);
          keyboardHandler.onKeyUp(previousLine);
          assertEquals('End', this.getRangeStart().name);
        })
        .call(() => {
          keyboardHandler.onKeyDown(previousObject);
          keyboardHandler.onKeyUp(previousObject);
          assertEquals('Start', this.getRangeStart().name);
        })
        .expectSpeech('Start')
        .replay();
  });
});

// Tests that a user can close ChromeVox (Ctrl + Alt + Z) when UserActionMonitor
// is active.
TEST_F('ChromeVoxUserActionMonitorTest', 'CloseChromeVox', function() {
  this.runWithLoadedTree(this.simpleDoc, function() {
    const keyboardHandler = new BackgroundKeyboardHandler();
    let finished = false;
    let closed = false;
    const actions =
        [{type: 'key_sequence', value: {'keys': {'keyCode': [KeyCode.A]}}}];
    const onFinished = () => finished = true;
    ChromeVoxState.instance.createUserActionMonitor(actions, onFinished);
    // Swap in the below function so we don't actually close ChromeVox.
    UserActionMonitor.closeChromeVox_ = () => {
      closed = true;
    };

    assertFalse(closed);
    assertFalse(finished);
    keyboardHandler.onKeyDown(
        this.createMockKeyDownEvent(KeyCode.CONTROL, {ctrlKey: true}));
    assertFalse(closed);
    assertFalse(finished);
    keyboardHandler.onKeyDown(this.createMockKeyDownEvent(
        KeyCode.ALT, {ctrlKey: true, altKey: true}));
    assertFalse(closed);
    assertFalse(finished);
    keyboardHandler.onKeyDown(
        this.createMockKeyDownEvent(KeyCode.Z, {ctrlKey: true, altKey: true}));
    assertTrue(closed);
    // |finished| remains false since we didn't press the expected key sequence.
    assertFalse(finished);
  });
});
