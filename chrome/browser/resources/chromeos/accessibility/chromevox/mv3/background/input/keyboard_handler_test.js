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

    globalThis.keyboardHandler = BackgroundKeyboardHandler.instance;

    // Swap in a mock for this function. In normal circumstances, the browser
    // will have a queue of pending events when this function is called.
    // However, this invariant is invalidated in this test suite since we are
    // calling directly into the BackgroundKeyboardHandler key handlers.
    chrome.accessibilityPrivate.processPendingSpokenFeedbackEvent =
        (id, propagate) => {};
  }

  callOnKeyDown(internalKeyEvent) {
    keyboardHandler.onKeyDown_(internalKeyEvent, () => {});
  }

  callOnKeyUp(internalKeyEvent) {
    keyboardHandler.onKeyUp_(internalKeyEvent, () => {});
  }
};


AX_TEST_F(
    'ChromeVoxBackgroundKeyboardHandlerTest', 'SearchGetsPassedThrough',
    async function() {
      await this.runWithLoadedTree('<p>test</p>');
      // A Search keydown gets eaten.
      const searchDown = {};
      searchDown.metaKey = true;
      this.callOnKeyDown(searchDown);
      assertEquals(1, keyboardHandler.eatenKeyDowns_.size);

      // A Search keydown does not get eaten when there's no range and there
      // was no previous range. TalkBack is handled elsewhere.
      ChromeVoxRange.set(null);
      ChromeVoxRange.instance.previous_ = null;
      const searchDown2 = {};
      searchDown2.metaKey = true;
      this.callOnKeyDown(searchDown2);
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
          TestUtils.createMockInternalKey(KeyCode.SEARCH, {metaKey: true});
      this.callOnKeyDown(search);
      assertEquals(1, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals('no_pass_through', keyboardHandler.passThroughState_);
      assertFalse(BackgroundKeyboardHandler.passThroughModeEnabled_);

      const searchShift = TestUtils.createMockInternalKey(
          KeyCode.SHIFT, {metaKey: true, shiftKey: true});
      this.callOnKeyDown(searchShift);
      assertEquals(2, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals('no_pass_through', keyboardHandler.passThroughState_);
      assertFalse(BackgroundKeyboardHandler.passThroughModeEnabled_);

      const searchShiftEsc = TestUtils.createMockInternalKey(
          KeyCode.ESCAPE, {metaKey: true, shiftKey: true});
      this.callOnKeyDown(searchShiftEsc);
      assertEquals(3, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_pass_through_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      this.callOnKeyUp(searchShiftEsc);
      assertEquals(2, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_pass_through_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      const searchShiftUp = TestUtils.createMockInternalKey(
          KeyCode.SHIFT, {metaKey: true, shiftKey: false});
      this.callOnKeyUp(searchShiftUp);
      assertEquals(1, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_pass_through_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      const searchUp =
          TestUtils.createMockInternalKey(KeyCode.SEARCH, {metaKey: false});
      this.callOnKeyUp(searchUp);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_shortcut_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      // Now, the next series of key downs should be passed through.
      // Try Search+Ctrl+M.
      this.callOnKeyDown(search);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(1, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_shortcut_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      const searchCtrl = TestUtils.createMockInternalKey(
          KeyCode.CONTROL, {metaKey: true, ctrlKey: true});
      this.callOnKeyDown(searchCtrl);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(2, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_shortcut_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      const searchCtrlM = TestUtils.createMockInternalKey(
          KeyCode.M, {metaKey: true, ctrlKey: true});
      this.callOnKeyDown(searchCtrlM);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(3, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_shortcut_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      this.callOnKeyUp(searchCtrlM);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(2, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_shortcut_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      this.callOnKeyUp(searchCtrl);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(1, keyboardHandler.passedThroughKeyDowns_.size);
      assertEquals(
          'pending_shortcut_keyups', keyboardHandler.passThroughState_);
      assertTrue(BackgroundKeyboardHandler.passThroughModeEnabled_);

      this.callOnKeyUp(search);
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
          TestUtils.createMockInternalKey(KeyCode.SEARCH, {metaKey: true});
      this.callOnKeyDown(search);
      assertNoPassThrough();

      const searchShift = TestUtils.createMockInternalKey(
          KeyCode.SHIFT, {metaKey: true, shiftKey: true});
      this.callOnKeyDown(searchShift);
      assertNoPassThrough();

      const searchShiftM = TestUtils.createMockInternalKey(
          KeyCode.M, {metaKey: true, shiftKey: true});
      this.callOnKeyDown(searchShiftM);
      assertNoPassThrough();

      this.callOnKeyUp(searchShiftM);
      assertNoPassThrough();

      this.callOnKeyUp(searchShift);
      assertNoPassThrough();

      this.callOnKeyUp(search);
      assertNoPassThrough();

      this.callOnKeyDown(TestUtils.createMockInternalKey(KeyCode.A));
      assertNoPassThrough();

      this.callOnKeyDown(
          TestUtils.createMockInternalKey(KeyCode.A, {altKey: true}));
      assertNoPassThrough();

      this.callOnKeyUp(
          TestUtils.createMockInternalKey(KeyCode.A, {altKey: true}));
      assertNoPassThrough();
    });

AX_TEST_F(
    'ChromeVoxBackgroundKeyboardHandlerTest', 'UnexpectedKeyDownUpPairs',
    async function() {
      await this.runWithLoadedTree('<p>test</p>');
      // Send a few key downs.
      const search =
          TestUtils.createMockInternalKey(KeyCode.SEARCH, {metaKey: true});
      this.callOnKeyDown(search);
      assertEquals(1, keyboardHandler.eatenKeyDowns_.size);

      const searchShift = TestUtils.createMockInternalKey(
          KeyCode.SHIFT, {metaKey: true, shiftKey: true});
      this.callOnKeyDown(searchShift);
      assertEquals(2, keyboardHandler.eatenKeyDowns_.size);

      const searchShiftM = TestUtils.createMockInternalKey(
          KeyCode.M, {metaKey: true, shiftKey: true});
      this.callOnKeyDown(searchShiftM);
      assertEquals(3, keyboardHandler.eatenKeyDowns_.size);

      // Now, send a key down, but no modifiers set, which is impossible to
      // actually press. This key is not eaten.
      const m = TestUtils.createMockInternalKey(KeyCode.M, {});
      this.callOnKeyDown(m);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);

      // To demonstrate eaten keys still work, send Search by itself, which is
      // always eaten.
      this.callOnKeyDown(search);
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
          TestUtils.createMockInternalKey(KeyCode.SEARCH, {metaKey: true});
      this.callOnKeyDown(search);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(1, keyboardHandler.passedThroughKeyDowns_.size);

      const searchShift = TestUtils.createMockInternalKey(
          KeyCode.SHIFT, {metaKey: true, shiftKey: true});
      this.callOnKeyDown(searchShift);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(2, keyboardHandler.passedThroughKeyDowns_.size);

      const searchShiftM = TestUtils.createMockInternalKey(
          KeyCode.M, {metaKey: true, shiftKey: true});
      this.callOnKeyDown(searchShiftM);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(3, keyboardHandler.passedThroughKeyDowns_.size);

      // Now, send a key down, but no modifiers set, which is impossible to
      // actually press. This is passed through, so the count resets to 1.
      const m = TestUtils.createMockInternalKey(KeyCode.M, {});
      this.callOnKeyDown(m);
      assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
      assertEquals(1, keyboardHandler.passedThroughKeyDowns_.size);
    });
