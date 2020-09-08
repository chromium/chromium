// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('GestureCommandData');
goog.provide('GestureGranularity');

goog.require('KeyCode');

/**
 * Map from gesture names (ax::mojom::Gesture defined in
 *     ui/accessibility/ax_enums.mojom.)  to commands.
 * @type {!Object<string,
 *     {
 *     msgId: string,
 *     command: (string|undefined),
 *     acceleratorAction:
 *      (chrome.accessibilityPrivate.AcceleratorAction|undefined),
 *     menuKeyOverride:
 *      ({keyCode: KeyCode, modifiers: ({ctrl: boolean}|undefined)}|undefined)
 *    }>}
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
  'swipeLeft2': {msgId: 'swipeleft2_gesture', command: 'previousWord'},
  'swipeRight2': {msgId: 'swiperight2_gesture', command: 'nextWord'},
  'swipeUp3': {msgId: 'swipeup3_gesture', command: 'nextPage'},
  'swipeDown3': {msgId: 'swipedown3_gesture', command: 'previousPage'},
  'swipeLeft3': {msgId: 'swipeleft3_gesture', command: 'previousGranularity'},
  'swipeRight3': {msgId: 'swiperight3_gesture', command: 'nextGranularity'},
  'swipeLeft4': {
    msgId: 'swipeleft4_gesture',
    acceleratorAction:
        chrome.accessibilityPrivate.AcceleratorAction.FOCUS_PREVIOUS_PANE
  },
  'swipeRight4': {
    msgId: 'swiperight4_gesture',
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
