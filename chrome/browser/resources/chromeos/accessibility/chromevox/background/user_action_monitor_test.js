// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for UserActionMonitor.
 */
ChromeVoxUserActionMonitorTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    await Promise.all([
      // Alphabetical based on file path.
      importModule(
          'ChromeVoxRange', '/chromevox/background/chromevox_range.js'),
      importModule(
          'BackgroundKeyboardHandler',
          '/chromevox/background/keyboard_handler.js'),
      importModule(
          'UserActionMonitor', '/chromevox/background/user_action_monitor.js'),
      importModule(
          'ChromeVoxKbHandler', '/chromevox/common/keyboard_handler.js'),
      importModule('KeySequence', '/chromevox/common/key_sequence.js'),
      importModule('KeyCode', '/common/key_code.js'),
    ]);

    globalThis.Gesture = chrome.accessibilityPrivate.Gesture;
  }

  /**
   * Returns the start node of the current ChromeVox range.
   * @return {AutomationNode}
   */
  getRangeStart() {
    return ChromeVoxRange.current.start.node;
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

AX_TEST_F('ChromeVoxUserActionMonitorTest', 'UnitTest', async function() {
  await this.runWithLoadedTree(this.simpleDoc);
  let finished = false;
  const actions = [
    {
      type: 'key_sequence',
      value: {'keys': {'keyCode': [KeyCode.SPACE]}},
    },
    {type: 'braille', value: 'jumpToTop'},
    {type: 'gesture', value: Gesture.SWIPE_UP1},
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

AX_TEST_F('ChromeVoxUserActionMonitorTest', 'ActionUnitTest', async function() {
  await this.runWithLoadedTree(this.simpleDoc);
  const keySequenceActionOne = UserActionMonitor.Action.fromActionInfo(
      {type: 'key_sequence', value: {keys: {keyCode: [KeyCode.SPACE]}}});
  const keySequenceActionTwo = new UserActionMonitor.Action({
    type: 'key_sequence',
    value: new KeySequence(TestUtils.createMockKeyEvent(KeyCode.A)),
  });
  const gestureActionOne = UserActionMonitor.Action.fromActionInfo(
      {type: 'gesture', value: Gesture.SWIPE_UP1});
  const gestureActionTwo =
      new UserActionMonitor.Action({type: 'gesture', value: Gesture.SWIPE_UP2});

  assertFalse(keySequenceActionOne.equals(keySequenceActionTwo));
  assertFalse(keySequenceActionOne.equals(gestureActionOne));
  assertFalse(keySequenceActionOne.equals(gestureActionTwo));
  assertFalse(keySequenceActionTwo.equals(gestureActionOne));
  assertFalse(keySequenceActionTwo.equals(gestureActionTwo));
  assertFalse(gestureActionOne.equals(gestureActionTwo));

  const cloneKeySequenceActionOne = UserActionMonitor.Action.fromActionInfo(
      {type: 'key_sequence', value: {keys: {keyCode: [KeyCode.SPACE]}}});
  const cloneGestureActionOne =
      new UserActionMonitor.Action({type: 'gesture', value: Gesture.SWIPE_UP1});
  assertTrue(keySequenceActionOne.equals(cloneKeySequenceActionOne));
  assertTrue(gestureActionOne.equals(cloneGestureActionOne));
});

AX_TEST_F('ChromeVoxUserActionMonitorTest', 'Errors', async function() {
  await this.runWithLoadedTree(this.simpleDoc);
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
    new UserActionMonitor.Action({type: 'key_sequence', value: 'invalid'});
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
    monitor.onKeySequence(
        new KeySequence(TestUtils.createMockKeyEvent(KeyCode.SPACE)));
    assertTrue(false);  // Shouldn't execute.
  } catch (error) {
    assertEquals('UserActionMonitor: actionIndex_ is invalid.', error.message);
    caught = true;
  }
  assertCaughtAndReset();
  try {
    monitor.expectedActionMatched_();
    assertTrue(false);  // Shouldn't execute.
  } catch (error) {
    assertEquals('UserActionMonitor: actionIndex_ is invalid.', error.message);
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

AX_TEST_F('ChromeVoxUserActionMonitorTest', 'Output', async function() {
  const mockFeedback = this.createMockFeedback();
  const rootNode = await this.runWithLoadedTree(this.simpleDoc);
  let monitor;
  let finished = false;
  const actions = [
    {
      type: 'gesture',
      value: Gesture.SWIPE_UP1,
      beforeActionMsg: 'First instruction',
      afterActionMsg: 'Congratulations!',
    },
    {
      type: 'gesture',
      value: Gesture.SWIPE_UP1,
      beforeActionMsg: 'Second instruction',
      afterActionMsg: 'You did it!',
    },
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
  await mockFeedback.replay();
});

// Tests that we can match a single key. Serves as an integration test
// since we don't directly call a UserActionMonitor function.
AX_TEST_F('ChromeVoxUserActionMonitorTest', 'SingleKey', async function() {
  await this.runWithLoadedTree(this.simpleDoc);
  const keyboardHandler = BackgroundKeyboardHandler.instance;
  let finished = false;
  const actions =
      [{type: 'key_sequence', value: {'keys': {'keyCode': [KeyCode.SPACE]}}}];
  const onFinished = () => finished = true;

  UserActionMonitor.create(actions, onFinished);
  keyboardHandler.onKeyDown(TestUtils.createMockKeyEvent(KeyCode.LEFT));
  keyboardHandler.onKeyUp(TestUtils.createMockKeyEvent(KeyCode.LEFT));
  assertFalse(finished);
  keyboardHandler.onKeyDown(TestUtils.createMockKeyEvent(KeyCode.RIGHT));
  keyboardHandler.onKeyUp(TestUtils.createMockKeyEvent(KeyCode.RIGHT));
  assertFalse(finished);
  keyboardHandler.onKeyDown(TestUtils.createMockKeyEvent(KeyCode.SPACE));
  keyboardHandler.onKeyUp(TestUtils.createMockKeyEvent(KeyCode.SPACE));
  assertTrue(finished);
});

// Tests that we can match a key sequence. Serves as an integration test
// since we don't directly call a UserActionMonitor function.
AX_TEST_F('ChromeVoxUserActionMonitorTest', 'MultipleKeys', async function() {
  await this.runWithLoadedTree(this.simpleDoc);
  const keyboardHandler = BackgroundKeyboardHandler.instance;
  let finished = false;
  const actions = [{
    type: 'key_sequence',
    value: {'cvoxModifier': true, 'keys': {'keyCode': [KeyCode.O, KeyCode.B]}},
  }];
  const onFinished = () => finished = true;

  UserActionMonitor.create(actions, onFinished);
  keyboardHandler.onKeyDown(TestUtils.createMockKeyEvent(KeyCode.O));
  keyboardHandler.onKeyUp(TestUtils.createMockKeyEvent(KeyCode.O));
  assertFalse(finished);
  keyboardHandler.onKeyDown(TestUtils.createMockKeyEvent(KeyCode.B));
  keyboardHandler.onKeyUp(TestUtils.createMockKeyEvent(KeyCode.B));
  assertFalse(finished);
  keyboardHandler.onKeyDown(TestUtils.createMockKeyEvent(KeyCode.SEARCH));
  keyboardHandler.onKeyUp(TestUtils.createMockKeyEvent(KeyCode.SEARCH));
  assertFalse(finished);
  keyboardHandler.onKeyDown(
      TestUtils.createMockKeyEvent(KeyCode.O, {searchKeyHeld: true}));
  assertFalse(finished);
  keyboardHandler.onKeyUp(
      TestUtils.createMockKeyEvent(KeyCode.O, {searchKeyHeld: true}));
  keyboardHandler.onKeyDown(TestUtils.createMockKeyEvent(KeyCode.B));
  assertTrue(finished);
});

// Tests that we can match multiple key sequences.
AX_TEST_F(
    'ChromeVoxUserActionMonitorTest', 'MultipleKeySequences', async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(this.simpleDoc);
      let finished = false;
      const actions = [
        {
          type: 'key_sequence',
          value: {
            'keys':
                {'altKey': [true], 'shiftKey': [true], 'keyCode': [KeyCode.L]},
          },
          afterActionMsg: 'You pressed the first sequence!',
        },
        {
          type: 'key_sequence',
          value: {
            'keys':
                {'altKey': [true], 'shiftKey': [true], 'keyCode': [KeyCode.S]},
          },
          afterActionMsg: 'You pressed the second sequence!',
        },
      ];
      const onFinished = () => finished = true;

      const altShiftLSequence = new KeySequence(TestUtils.createMockKeyEvent(
          KeyCode.L, {altKey: true, shiftKey: true}));
      const altShiftSSequence = new KeySequence(TestUtils.createMockKeyEvent(
          KeyCode.S, {altKey: true, shiftKey: true}));
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
      await mockFeedback.replay();
    });

// Tests that we can provide expectations for ChromeVox commands and block
// command execution until the desired command is performed. Serves as an
// integration test since we don't directly call a UserActionMonitor function.
AX_TEST_F('ChromeVoxUserActionMonitorTest', 'BlockCommands', async function() {
  const mockFeedback = this.createMockFeedback();
  await this.runWithLoadedTree(this.paragraphDoc);
  const keyboardHandler = BackgroundKeyboardHandler.instance;
  let finished = false;
  const actions = [
    {
      type: 'key_sequence',
      value: {'cvoxModifier': true, 'keys': {'keyCode': [KeyCode.RIGHT]}},
    },
    {
      type: 'key_sequence',
      value: {'cvoxModifier': true, 'keys': {'keyCode': [KeyCode.LEFT]}},
    },
  ];
  const onFinished = () => finished = true;

  const nextObject =
      TestUtils.createMockKeyEvent(KeyCode.RIGHT, {searchKeyHeld: true});
  const nextLine =
      TestUtils.createMockKeyEvent(KeyCode.DOWN, {searchKeyHeld: true});
  const previousObject =
      TestUtils.createMockKeyEvent(KeyCode.LEFT, {searchKeyHeld: true});
  const previousLine =
      TestUtils.createMockKeyEvent(KeyCode.UP, {searchKeyHeld: true});

  UserActionMonitor.create(actions, onFinished);
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
      .expectSpeech('Start');
  await mockFeedback.replay();
});

// Tests that a user can close ChromeVox (Ctrl + Alt + Z) when UserActionMonitor
// is active.
AX_TEST_F('ChromeVoxUserActionMonitorTest', 'CloseChromeVox', async function() {
  await this.runWithLoadedTree(this.simpleDoc);
  const keyboardHandler = BackgroundKeyboardHandler.instance;
  let finished = false;
  let closed = false;
  const actions =
      [{type: 'key_sequence', value: {'keys': {'keyCode': [KeyCode.A]}}}];
  const onFinished = () => finished = true;
  UserActionMonitor.create(actions, onFinished);
  // Swap in the below function so we don't actually close ChromeVox.
  UserActionMonitor.closeChromeVox_ = () => {
    closed = true;
  };

  assertFalse(closed);
  assertFalse(finished);
  keyboardHandler.onKeyDown(
      TestUtils.createMockKeyEvent(KeyCode.CONTROL, {ctrlKey: true}));
  assertFalse(closed);
  assertFalse(finished);
  keyboardHandler.onKeyDown(
      TestUtils.createMockKeyEvent(KeyCode.ALT, {ctrlKey: true, altKey: true}));
  assertFalse(closed);
  assertFalse(finished);
  keyboardHandler.onKeyDown(
      TestUtils.createMockKeyEvent(KeyCode.Z, {ctrlKey: true, altKey: true}));
  assertTrue(closed);
  // |finished| remains false since we didn't press the expected key
  // sequence.
  assertFalse(finished);
});

// Tests that we can stop propagation of an action, even if it is matched.
// In this test, we stop propagation of the Control key to avoid executing the
// stopSpeech command.
AX_TEST_F(
    'ChromeVoxUserActionMonitorTest', 'StopPropagation', async function() {
      await this.runWithLoadedTree(this.simpleDoc);
      const keyboardHandler = BackgroundKeyboardHandler.instance;
      let finished = false;
      let executedCommand = false;
      const actions = [{
        type: 'key_sequence',
        value: {keys: {keyCode: [KeyCode.CONTROL]}},
        shouldPropagate: false,
      }];
      const onFinished = () => finished = true;
      UserActionMonitor.create(actions, onFinished);
      ChromeVoxKbHandler.commandHandler = command => executedCommand = true;
      assertFalse(finished);
      assertFalse(executedCommand);
      keyboardHandler.onKeyDown(TestUtils.createMockKeyEvent(KeyCode.CONTROL));
      keyboardHandler.onKeyUp(TestUtils.createMockKeyEvent(KeyCode.CONTROL));
      assertFalse(executedCommand);
      assertTrue(finished);
    });

// Tests that we can match a gesture when it's performed.
AX_TEST_F('ChromeVoxUserActionMonitorTest', 'Gestures', async function() {
  await this.runWithLoadedTree(this.simpleDoc);
  let finished = false;
  const actions = [{type: 'gesture', value: Gesture.SWIPE_RIGHT1}];
  const onFinished = () => finished = true;

  UserActionMonitor.create(actions, onFinished);
  doGesture(Gesture.SWIPE_LEFT1)();
  assertFalse(finished);
  doGesture(Gesture.SWIPE_LEFT2)();
  assertFalse(finished);
  doGesture(Gesture.SWIPE_RIGHT1)();
  assertTrue(finished);
});

// Tests that we can perform a command when an action has been matched.
AX_TEST_F(
    'ChromeVoxUserActionMonitorTest', 'AfterActionCommand', async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(this.simpleDoc);
      let finished = false;
      const actions = [{
        type: 'gesture',
        value: Gesture.SWIPE_RIGHT1,
        afterActionCmd: 'announceBatteryDescription',
      }];
      const onFinished = () => finished = true;

      UserActionMonitor.create(actions, onFinished);
      mockFeedback
          .call(() => {
            doGesture(Gesture.SWIPE_RIGHT1)();
            assertTrue(finished);
          })
          .expectSpeech(/Battery at [0-9]+ percent/);
      await mockFeedback.replay();
    });
