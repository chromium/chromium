// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('GestureCommandData');
goog.provide('GestureGranularity');

goog.require('KeyCode');

/**
 * Map from gesture names (ax::mojom::Gesture defined in
 *     ui/accessibility/ax_enums.mojom.)  to commands.
 *
 * Note that only one of |command|, |acceleratorAction|, or |globalKey| is
 * expected.
 * @type {!Object<string, {msgId: string, command: (string|undefined),
 *     commandDescriptionMsgId: (string|undefined),
 *     acceleratorAction:
 *     (chrome.accessibilityPrivate.AcceleratorAction|undefined),
 *     globalKey: ({keyCode: !KeyCode, modifiers:
 *     (chrome.accessibilityPrivate.SyntheticKeyboardModifiers|undefined)}|undefined),
 *     menuKeyOverride: ({keyCode: !KeyCode, modifiers:
 *     (chrome.accessibilityPrivate.SyntheticKeyboardModifiers|undefined)}|undefined)}>}
 * @const
 */
GestureCommandData.GESTURE_COMMAND_MAP = {
  'click': {command: 'forceClickOnCurrentItem', msgId: 'click_gesture'},
  'swipeUp1': {
    msgId: 'swipeup1_gesture',
    command: 'previousAtGranularity',
    menuKeyOverride: {keyCode: KeyCode.UP}
  },
  'swipeDown1': {
    msgId: 'swipedown1_gesture',
    command: 'nextAtGranularity',
    menuKeyOverride: {keyCode: KeyCode.DOWN}
  },
  'swipeLeft1': {
    msgId: 'swipeleft1_gesture',
    command: 'previousObject',
    menuKeyOverride: {keyCode: KeyCode.LEFT}
  },
  'swipeRight1': {
    msgId: 'swiperight1_gesture',
    command: 'nextObject',
    menuKeyOverride: {keyCode: KeyCode.RIGHT}
  },
  'swipeUp2': {msgId: 'swipeup2_gesture', command: 'jumpToTop'},
  'swipeDown2': {msgId: 'swipedown2_gesture', command: 'readFromHere'},
  'swipeLeft2': {
    msgId: 'swipeleft2_gesture',
    commandDescriptionMsgId: 'escape_gesture_description',
    globalKey: {keyCode: KeyCode.ESCAPE}
  },
  'swipeRight2': {
    msgId: 'swiperight2_gesture',
    commandDescriptionMsgId: 'enter_gesture_description',
    globalKey: {keyCode: KeyCode.RETURN}
  },
  'swipeUp3': {
    msgId: 'swipeup3_gesture',
    commandDescriptionMsgId: 'next_page_gesture_description',
    command: 'nextPage'
  },
  'swipeDown3': {
    msgId: 'swipedown3_gesture',
    commandDescriptionMsgId: 'previous_page_gesture_description',
    command: 'previousPage'
  },
  'swipeLeft3': {msgId: 'swipeleft3_gesture', command: 'previousGranularity'},
  'swipeRight3': {msgId: 'swiperight3_gesture', command: 'nextGranularity'},
  'swipeLeft4': {
    msgId: 'swipeleft4_gesture',
    commandDescriptionMsgId: 'previous_pane_gesture_description',
    acceleratorAction:
        chrome.accessibilityPrivate.AcceleratorAction.FOCUS_PREVIOUS_PANE
  },
  'swipeRight4': {
    msgId: 'swiperight4_gesture',
    commandDescriptionMsgId: 'next_pane_gesture_description',
    acceleratorAction:
        chrome.accessibilityPrivate.AcceleratorAction.FOCUS_NEXT_PANE
  },

  'touchExplore': {msgId: 'touch_explore_gesture'},

  'tap2': {msgId: 'tap2_gesture', command: 'stopSpeech'},
  'tap4': {msgId: 'tap4_gesture', command: 'showPanelMenuMostRecent'},
};

/**
 * Possible granularities to navigate.
 * @enum {number}
 */
GestureGranularity = {
  CHARACTER: 0,
  WORD: 1,
  LINE: 2,
  HEADING: 3,
  LINK: 4,
  FORM_FIELD_CONTROL: 5,
  COUNT: 6
};
