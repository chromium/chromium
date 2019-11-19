// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('GestureCommandData');
goog.provide('GestureGranularity');

/**
 * Map from gesture names (ax::mojom::Gesture defined in
 *     ui/accessibility/ax_enums.mojom.)  to commands.
 * @type {!Object<string,
 *     {
 *     msgId: string,
 *     command: string,
 *     menuKeyOverride: (boolean|undefined),
 *     keyOverride: ({keyCode: number, modifiers: ({ctrl:
 * boolean}|undefined)}|undefined)
 *    }>}
 * @const
 */
GestureCommandData.GESTURE_COMMAND_MAP = {
  'click': {command: 'forceClickOnCurrentItem', msgId: 'click_gesture'},
  'swipeUp1': {
    msgId: 'swipeUp1_gesture',
    command: 'previousAtGranularity',
    menuKeyOverride: true,
    keyOverride: {keyCode: 38 /* up */, skipStart: true, multiline: true}
  },
  'swipeDown1': {
    msgId: 'swipeDown1_gesture',
    command: 'nextAtGranularity',
    menuKeyOverride: true,
    keyOverride: {keyCode: 40 /* Down */, skipEnd: true, multiline: true}
  },
  'swipeLeft1': {
    msgId: 'swipeLeft1_gesture',
    command: 'previousObject',
    menuKeyOverride: true,
    keyOverride: {keyCode: 37 /* left */}
  },
  'swipeRight1': {
    msgId: 'swipeRight1_gesture',
    command: 'nextObject',
    menuKeyOverride: true,
    keyOverride: {keyCode: 39 /* right */}
  },
  'swipeUp2': {msgId: 'swipeUp2_gesture', command: 'jumpToTop'},
  'swipeDown2': {msgId: 'swipeDown2_gesture', command: 'readFromHere'},
  'swipeLeft2': {
    msgId: 'swipeLeft2_gesture',
    command: 'previousWord',
    keyOverride: {keyCode: 37 /* left */, modifiers: {ctrl: true}}
  },
  'swipeRight2': {
    msgId: 'swipeRight2_gesture',
    command: 'nextWord',
    keyOverride: {keyCode: 40 /* right */, modifiers: {ctrl: true}}
  },
  'swipeUp3': {msgId: 'swipeUp3_gesture', command: 'scrollForward'},
  'swipeDown3': {msgId: 'swipeDown3_gesture', command: 'scrollBackward'},
  'swipeLeft3': {msgId: 'swipeLeft3_gesture', command: 'previousGranularity'},
  'swipeRight3': {msgId: 'swipeRight3_gesture', command: 'nextGranularity'},

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
  COUNT: 3
};
