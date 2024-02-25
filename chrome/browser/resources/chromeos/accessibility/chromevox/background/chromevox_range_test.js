// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/documents.js']);

/** Test fixture for ChromeVoxRange. */
ChromeVoxRangeTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    globalThis.RoleType = chrome.automation.RoleType;
  }
};

AX_TEST_F('ChromeVoxRangeTest', 'Observer', async function() {
  const root =
      await this.runWithLoadedTree(Documents.button + Documents.slider);
  const button = root.find({role: RoleType.BUTTON});
  assertNotNullNorUndefined(button);
  const slider = root.find({role: RoleType.SLIDER});
  assertNotNullNorUndefined(slider);

  const observer = new class extends ChromeVoxRangeObserver {
    onCurrentRangeChanged(range, opt_fromEditing) {
      this.capturedRange = range;
      this.capturedFromEditing = opt_fromEditing;
    }
  }
  ();
  ChromeVoxRange.addObserver(observer);

  // Test when range changes not from editing.
  const buttonRange = CursorRange.fromNode(button);
  ChromeVoxRange.set(buttonRange);
  assertEquals(buttonRange, observer.capturedRange);
  assertUndefined(observer.capturedFromEditing);

  // Test when range change is from editing.
  const sliderRange = CursorRange.fromNode(slider);
  ChromeVoxRange.set(sliderRange, true);
  assertEquals(sliderRange, observer.capturedRange);
  assertTrue(observer.capturedFromEditing);

  // Test removeObserver.
  ChromeVoxRange.removeObserver(observer);
  delete observer.capturedRange;
  ChromeVoxRange.set(buttonRange);
  assertUndefined(observer.capturedRange);
});

AX_TEST_F(
    'ChromeVoxRangeTest', 'GetCurrentRangeWithoutRecovery', async function() {
      const root = await this.runWithLoadedTree('');
      ChromeVoxRange.instance.current_ = CursorRange.fromNode(root);
      ChromeVoxRange.instance.current_.isValid = () => false;

      assertNotNullNorUndefined(
          ChromeVoxRange.getCurrentRangeWithoutRecovery());
    });

AX_TEST_F('ChromeVoxRangeTest', 'Current', async function() {
  const root = await this.runWithLoadedTree('');
  ChromeVoxRange.instance.current_ = null;
  assertEquals(null, ChromeVoxRange.current, 'First');

  ChromeVoxRange.instance.current_ = CursorRange.fromNode(root);
  assertEquals(
      ChromeVoxRange.instance.current_, ChromeVoxRange.current, 'Second');

  ChromeVoxRange.instance.current_.isValid = () => false;
  assertEquals(null, ChromeVoxRange.current, 'Third');
});

AX_TEST_F('ChromeVoxRangeTest', 'Set', async function() {
  const root = await this.runWithLoadedTree(Documents.button);
  const rootRange = CursorRange.fromNode(root);
  const button = root.find({role: RoleType.BUTTON});
  assertNotNullNorUndefined(button);
  const buttonRange = CursorRange.fromNode(button);
  const desktopRange = CursorRange.fromNode(this.desktop_);

  let hasThawed;
  ChromeVox.braille.thaw = () => hasThawed = true;
  let lastFocusBounds;
  FocusBounds.set = bounds => lastFocusBounds = bounds;

  const reset = () => {
    hasThawed = false;
    lastFocusBounds = null;
    ChromeVoxRange.instance.current_ = null;
    ChromeVoxRange.instance.previous_ = rootRange;
    ChromeVoxState.position = {};
  };

  // When setting to null and the previous is already null, it returns early.
  reset();
  ChromeVoxRange.instance.current_ = null;
  ChromeVoxRange.set(null);
  assertTrue(hasThawed);
  assertTrue(lastFocusBounds instanceof Array);
  assertEquals(0, lastFocusBounds.length);
  assertNotNullNorUndefined(ChromeVoxRange.instance.previous_);
  assertEquals(0, Object.keys(ChromeVoxState.position).length);

  // When the new range is not valid, it returns early.
  reset();
  const invalidRange = CursorRange.fromNode(root);
  invalidRange.isValid = () => false;
  ChromeVoxRange.set(invalidRange);
  assertTrue(hasThawed);
  assertTrue(lastFocusBounds instanceof Array);
  assertEquals(0, lastFocusBounds.length);
  assertEquals(0, Object.keys(ChromeVoxState.position).length);

  // When the new range is null, set the previous and current and then return.
  reset();
  ChromeVoxRange.instance.current_ = buttonRange;
  ChromeVoxRange.set(null);
  assertTrue(hasThawed);
  assertTrue(lastFocusBounds instanceof Array);
  assertEquals(0, lastFocusBounds.length);
  assertEquals(buttonRange, ChromeVoxRange.instance.previous_);
  assertEquals(null, ChromeVoxRange.instance.current_);
  assertEquals(0, Object.keys(ChromeVoxState.position).length);

  // When the node's root is the desktop, don't set a position.
  reset();
  ChromeVoxRange.set(desktopRange);
  assertTrue(hasThawed);
  assertEquals(null, ChromeVoxRange.instance.previous_);
  assertEquals(desktopRange, ChromeVoxRange.instance.current_);
  assertEquals(0, Object.keys(ChromeVoxState.position).length);

  // Check that the position is set when the focus is in a webpage.
  reset();
  ChromeVoxRange.set(buttonRange);
  assertTrue(hasThawed);
  assertEquals(null, ChromeVoxRange.instance.previous_);
  assertEquals(buttonRange, ChromeVoxRange.instance.current_);
  assertEquals(1, Object.keys(ChromeVoxState.position).length);
});

TEST_F('ChromeVoxRangeTest', 'MaybeResetFromFocus', async function() {
  const root = await this.runWithLoadedTree(Documents.button);
  const button = root.find({role: RoleType.BUTTON});

  let focusNode = null;
  chrome.automation.getFocus = callback => callback(focusNode);

  // If there's no focused node, the range is set to null.
  ChromeVoxRange.instance.current_ = CursorRange.fromNode(button);
  ChromeVoxRange.maybeResetFromFocus();
  assertEquals(null, ChromeVoxRange.instance.current_);

  // If the current node is nod valid and there's a current focus, set the
  // current range to be the focus.
  const invalidRange = CursorRange.fromNode(button);
  invalidRange.isValid = () => false;
  ChromeVoxRange.instance.current_ = invalidRange;
  focusNode = root;
  ChromeVoxRange.maybeResetFromFocus();
  assertEquals(focusNode, ChromeVoxRange.instance.current_);

  // If talkback is enabled, clear the range.
  ChromeVoxState.instance.talkBackEnabled = true;
  focusNode = this.desktop_.find({role: RoleType.CLIENT});
  assertNotNullNorUndefined(focusNode);
  ChromeVoxRange.maybeResetFromFocus();
  assertEquals(null, ChromeVoxRange.instance.current_);
});
