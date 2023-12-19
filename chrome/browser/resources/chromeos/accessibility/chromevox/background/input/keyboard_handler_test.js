// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for ChromeVox KeyboardHandler.
 */
ChromeVoxBackgroundKeyboardHandlerTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    await Promise.all([
      // Alphabetical based on file path.
      importModule('ChromeVox', '/chromevox/background/chromevox.js'),
      importModule(
          'ChromeVoxRange', '/chromevox/background/chromevox_range.js'),
      importModule(
          'ChromeVoxState', '/chromevox/background/chromevox_state.js'),
      importModule(
          'BackgroundKeyboardHandler',
          '/chromevox/background/input/background_keyboard_handler.js'),
      importModule('KeyCode', '/common/key_code.js'),
    ]);

    globalThis.keyboardHandler = BackgroundKeyboardHandler.instance;
  }
};


AX_TEST_F(
    'ChromeVoxBackgroundKeyboardHandlerTest', 'SearchGetsPassedThrough',
    async function() {
      await this.runWithLoadedTree('<p>test</p>');
      // A Search keydown gets eaten.
      const searchDown = {};
      searchDown.preventDefault = this.newCallback();
      searchDown.stopPropagation = this.newCallback();
      searchDown.metaKey = true;
      keyboardHandler.onKeyDown(searchDown);
      assertEquals(1, keyboardHandler.eatenKeyDowns_.size);

      // A Search keydown does not get eaten when there's no range and there
      // was no previous range. TalkBack is handled elsewhere.
      ChromeVoxRange.set(null);
      ChromeVoxRange.instance.previous_ = null;
      const searchDown2 = {};
      searchDown2.metaKey = true;
      keyboardHandler.onKeyDown(searchDown2);
      assertEquals(1, keyboardHandler.eatenKeyDowns_.size);
    });

AX_TEST_F(
    'ChromeVoxBackgroundKeyboardHandlerTest', 'PassThroughMode',
    async function() {
      await this.runWithLoadedTree('<p>test</p>');
      assertFalse(BackgroundKeyboardHandler.passThroughModeEnabled_);
      assertEquals('no_pass_through', keyboardHandler.passThroughState_);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);

      // Send the pass through command: Search+Shift+Escape.
      const search =
          TestUtils.createMockKeyEvent(KeyCode.SEARCH, {metaKey: true});
      keyboardHandler.onKeyDown(search);
      assertEquals(1, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals('no_pass_through', keyboardHandler.passThroughState_);
      assertFalse(BackgroundKeyboardHandler.passThroughModeEnabled_);

      const searchShift = TestUtils.createMockKeyEvent(
          KeyCode.SHIFT, {metaKey: true, shiftKey: true});
      keyboardHandler.onKeyDown(searchShift);
      assertEquals(2, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals('no_pass_through', keyboardHandler.passThroughState_);
      assertFalse(BackgroundKeyboardHandler.passThroughModeEnabled_);

      const searchShiftEsc = TestUtils.createMockKeyEvent(
          KeyCode.ESCAPE, {metaKey: true, shiftKey: true});
      keyboardHandler.onKeyDown(searchShiftEsc);
      assertEquals(3, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_pass_through_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      keyboardHandler.onKeyUp(searchShiftEsc);
      assertEquals(2, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_pass_through_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      const searchShiftUp = TestUtils.createMockKeyEvent(
          KeyCode.SHIFT, {metaKey: true, shiftKey: false});
      keyboardHandler.onKeyUp(searchShiftUp);
      assertEquals(1, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_pass_through_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      const searchUp =
          TestUtils.createMockKeyEvent(KeyCode.SEARCH, {metaKey: false});
      keyboardHandler.onKeyUp(searchUp);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_shortcut_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      // Now, the next series of key downs should be passed through.
      // Try Search+Ctrl+M.
      keyboardHandler.onKeyDown(search);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(1, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_shortcut_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      const searchCtrl = TestUtils.createMockKeyEvent(
          KeyCode.CONTROL, {metaKey: true, ctrlKey: true});
      keyboardHandler.onKeyDown(searchCtrl);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(2, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_shortcut_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      const searchCtrlM = TestUtils.createMockKeyEvent(
          KeyCode.M, {metaKey: true, ctrlKey: true});
      keyboardHandler.onKeyDown(searchCtrlM);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(3, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_shortcut_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      keyboardHandler.onKeyUp(searchCtrlM);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(2, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_shortcut_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      keyboardHandler.onKeyUp(searchCtrl);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(1, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_shortcut_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      keyboardHandler.onKeyUp(search);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals('no_pass_through', keyboardHandler.passThroughState_);
      assertFalse(BackgroundKeyboardHandler.passThroughModeEnabled_);
    });

AX_TEST_F(
    'ChromeVoxBackgroundKeyboardHandlerTest', 'PassThroughModeOff',
    async function() {
      await this.runWithLoadedTree('<p>test</p>');
      function assertNoPassThrough() {
        assertFalse(BackgroundKeyboardHandler.passThroughModeEnabled_);
        assertEquals('no_pass_through', keyboardHandler.passThroughState_);
        assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
      }

      // Send some random keys; ensure the pass through state variables never
      // change.
      const search =
          TestUtils.createMockKeyEvent(KeyCode.SEARCH, {metaKey: true});
      keyboardHandler.onKeyDown(search);
      assertNoPassThrough();

      const searchShift = TestUtils.createMockKeyEvent(
          KeyCode.SHIFT, {metaKey: true, shiftKey: true});
      keyboardHandler.onKeyDown(searchShift);
      assertNoPassThrough();

      const searchShiftM = TestUtils.createMockKeyEvent(
          KeyCode.M, {metaKey: true, shiftKey: true});
      keyboardHandler.onKeyDown(searchShiftM);
      assertNoPassThrough();

      keyboardHandler.onKeyUp(searchShiftM);
      assertNoPassThrough();

      keyboardHandler.onKeyUp(searchShift);
      assertNoPassThrough();

      keyboardHandler.onKeyUp(search);
      assertNoPassThrough();

      keyboardHandler.onKeyDown(TestUtils.createMockKeyEvent(KeyCode.A));
      assertNoPassThrough();

      keyboardHandler.onKeyDown(
          TestUtils.createMockKeyEvent(KeyCode.A, {altKey: true}));
      assertNoPassThrough();

      keyboardHandler.onKeyUp(
          TestUtils.createMockKeyEvent(KeyCode.A, {altKey: true}));
      assertNoPassThrough();
    });

AX_TEST_F(
    'ChromeVoxBackgroundKeyboardHandlerTest', 'UnexpectedKeyDownUpPairs',
    async function() {
      await this.runWithLoadedTree('<p>test</p>');
      // Send a few key downs.
      const search =
          TestUtils.createMockKeyEvent(KeyCode.SEARCH, {metaKey: true});
      keyboardHandler.onKeyDown(search);
      assertEquals(1, keyboardHandler.eatenKeyDowns_.size);

      const searchShift = TestUtils.createMockKeyEvent(
          KeyCode.SHIFT, {metaKey: true, shiftKey: true});
      keyboardHandler.onKeyDown(searchShift);
      assertEquals(2, keyboardHandler.eatenKeyDowns_.size);

      const searchShiftM = TestUtils.createMockKeyEvent(
          KeyCode.M, {metaKey: true, shiftKey: true});
      keyboardHandler.onKeyDown(searchShiftM);
      assertEquals(3, keyboardHandler.eatenKeyDowns_.size);

      // Now, send a key down, but no modifiers set, which is impossible to
      // actually press. This key is not eaten.
      const m = TestUtils.createMockKeyEvent(KeyCode.M, {});
      keyboardHandler.onKeyDown(m);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);

      // To demonstrate eaten keys still work, send Search by itself, which is
      // always eaten.
      keyboardHandler.onKeyDown(search);
      assertEquals(1, keyboardHandler.eatenKeyDowns_.size);
    });

AX_TEST_F(
    'ChromeVoxBackgroundKeyboardHandlerTest',
    'UnexpectedKeyDownUpPairsPassThrough', async function() {
      await this.runWithLoadedTree('<p>test</p>');
      // Force pass through mode.
      BackgroundKeyboardHandler.passThroughModeEnabled_ = true;

      // Send a few key downs (which are passed through).
      const search =
          TestUtils.createMockKeyEvent(KeyCode.SEARCH, {metaKey: true});
      keyboardHandler.onKeyDown(search);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(1, keyboardHandler.passedThroughKeyDowns_.size);

      const searchShift = TestUtils.createMockKeyEvent(
          KeyCode.SHIFT, {metaKey: true, shiftKey: true});
      keyboardHandler.onKeyDown(searchShift);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(2, keyboardHandler.passedThroughKeyDowns_.size);

      const searchShiftM = TestUtils.createMockKeyEvent(
          KeyCode.M, {metaKey: true, shiftKey: true});
      keyboardHandler.onKeyDown(searchShiftM);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(3, keyboardHandler.passedThroughKeyDowns_.size);

      // Now, send a key down, but no modifiers set, which is impossible to
      // actually press. This is passed through, so the count resets to 1.
      const m = TestUtils.createMockKeyEvent(KeyCode.M, {});
      keyboardHandler.onKeyDown(m);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(1, keyboardHandler.passedThroughKeyDowns_.size);
    });
