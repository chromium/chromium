// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Helper functions for tests driven from JS, especially SAATLite tests.
 * Must call `await TestUtility.setup()` before using these functions.
 *
 * Available functions:
 *     expectFocusOn(descriptor) - waits for Switch Access focus to land on a
 *         node with properties exactly matching those in the descriptor.
 *     pressNextSwitch()
 *     pressPreviousSwitch()
 *     pressSelectSwitch()
 *     startFocusInside(rootWebArea) - moves focus to the first interesting
 *         descendant of rootWebArea.
 */
const TestUtility = {
  setup() {
    FocusRingManager.instance.observer_ = TestUtility.whenFocusChanges_;
  },

  currentFocus: {primary: null, preview: null},

  async expectFocusOn(descriptor) {
    const current = TestUtility.currentFocus.primary;
    if (current && TestUtility.matches_(current, descriptor)) {
      return;
    }

    TestUtility.waitingForFocusDescriptor_ = descriptor;
    await new Promise(
        resolve => TestUtility.waitingForFocusResolver_ = resolve);
  },

  pressNextSwitch() {
    SACommands.instance.runCommand_(
        chrome.accessibilityPrivate.SwitchAccessCommand.NEXT);
  },

  pressPreviousSwitch() {
    SACommands.instance.runCommand_(
        chrome.accessibilityPrivate.SwitchAccessCommand.PREVIOUS);
  },

  pressSelectSwitch() {
    SACommands.instance.runCommand_(
        chrome.accessibilityPrivate.SwitchAccessCommand.SELECT);
  },

  /** Only call after runWithLoadedTree() */
  startFocusInside(rootWebArea) {
    if (!rootWebArea) {
      throw new Error('Web root node is undefined');
    }
    if (!SwitchAccessPredicate.isInterestingSubtree(rootWebArea)) {
      Navigator.byItem.moveTo_(rootWebArea);
      return;
    }

    let node = rootWebArea;
    while (!SwitchAccessPredicate.isInteresting(node)) {
      for (const child of node.children) {
        if (SwitchAccessPredicate.isInterestingSubtree(child)) {
          node = child;
          break;
        }
      }
    }

    Navigator.byItem.moveTo_(node);
  },

  // =============== Private Functions ==============

  /** @private */
  matches_(node, descriptor) {
    for (const key of Object.keys(descriptor)) {
      if (node[key] !== descriptor[key]) {
        return false;
      }
    }
    return true;
  },

  /** @private */
  whenFocusChanges_(primary, preview) {
    TestUtility.currentFocus.primary = primary;
    TestUtility.currentFocus.preview = preview;

    if (TestUtility.waitingForFocusResolver_ &&
        TestUtility.matches_(primary, TestUtility.waitingForFocusDescriptor_)) {
      TestUtility.waitingForFocusResolver_();

      TestUtility.waitingForFocusDescriptor_ = null;
      TestUtility.waitingForFocusResolver_ = null;
    }
  },

  /** @private */
  waitingForFocusDescriptor_: null,

  /** @private */
  waitingForFocusResolver_: null,
};
