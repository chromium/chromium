// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KeyCode} from '/common/key_code.js';

import {Command} from './command.js';

type AcceleratorAction = chrome.accessibilityPrivate.AcceleratorAction;
type SyntheticKeyboardModifiers =
    chrome.accessibilityPrivate.SyntheticKeyboardModifiers;

interface KeyData {
  keyCode: KeyCode;
  modifiers?: SyntheticKeyboardModifiers;
}

interface DataEntry {
  acceleratorAction?: AcceleratorAction;
  command?: Command;
  commandDescriptionMsgId?: string;
  globalKey?: KeyData;
  menuKeyOverride?: KeyData;
  msgId: string;
}

export namespace GestureCommandData {
  /**
   * Map from gesture names (ax::mojom::Gesture defined in
   *     ui/accessibility/ax_enums.mojom.)  to commands.
   *
   * Note that only one of |command|, |acceleratorAction|, or |globalKey| is
   * expected.
   */
  export const GESTURE_COMMAND_MAP: Record<string, DataEntry> = {
    'click':
        {command: Command.FORCE_CLICK_ON_CURRENT_ITEM, msgId: 'click_gesture'},
    'swipeUp1': {
      msgId: 'swipeup1_gesture',
      command: Command.PREVIOUS_AT_GRANULARITY,
      menuKeyOverride: {keyCode: KeyCode.UP},
    },
    'swipeDown1': {
      msgId: 'swipedown1_gesture',
      command: Command.NEXT_AT_GRANULARITY,
      menuKeyOverride: {keyCode: KeyCode.DOWN},
    },
    'swipeLeft1': {
      msgId: 'swipeleft1_gesture',
      command: Command.PREVIOUS_OBJECT,
      menuKeyOverride: {keyCode: KeyCode.LEFT},
    },
    'swipeRight1': {
      msgId: 'swiperight1_gesture',
      command: Command.NEXT_OBJECT,
      menuKeyOverride: {keyCode: KeyCode.RIGHT},
    },
    'swipeUp2': {msgId: 'swipeup2_gesture', command: Command.JUMP_TO_TOP},
    'swipeDown2':
        {msgId: 'swipedown2_gesture', command: Command.READ_FROM_HERE},
    'swipeLeft2': {
      msgId: 'swipeleft2_gesture',
      commandDescriptionMsgId: 'escape_gesture_description',
      globalKey: {keyCode: KeyCode.ESCAPE},
    },
    'swipeRight2': {
      msgId: 'swiperight2_gesture',
      commandDescriptionMsgId: 'enter_gesture_description',
      globalKey: {keyCode: KeyCode.RETURN},
    },
    'swipeUp3': {
      msgId: 'swipeup3_gesture',
      commandDescriptionMsgId: 'next_page_gesture_description',
      command: Command.NEXT_PAGE,
    },
    'swipeDown3': {
      msgId: 'swipedown3_gesture',
      commandDescriptionMsgId: 'previous_page_gesture_description',
      command: Command.PREVIOUS_PAGE,
    },
    'swipeLeft3':
        {msgId: 'swipeleft3_gesture', command: Command.PREVIOUS_GRANULARITY},
    'swipeRight3':
        {msgId: 'swiperight3_gesture', command: Command.NEXT_GRANULARITY},
    'swipeLeft4': {
      msgId: 'swipeleft4_gesture',
      commandDescriptionMsgId: 'previous_pane_gesture_description',
      acceleratorAction: chrome.accessibilityPrivate.AcceleratorAction
                           .FOCUS_PREVIOUS_PANE,
    },
    'swipeRight4': {
      msgId: 'swiperight4_gesture',
      commandDescriptionMsgId: 'next_pane_gesture_description',
      acceleratorAction: chrome.accessibilityPrivate.AcceleratorAction
                           .FOCUS_NEXT_PANE,
    },

    'touchExplore': {msgId: 'touch_explore_gesture'},

    'tap2': {msgId: 'tap2_gesture', command: Command.STOP_SPEECH},
    'tap4':
        {msgId: 'tap4_gesture', command: Command.SHOW_PANEL_MENU_MOST_RECENT},
  };
}

/** Possible granularities to navigate. */
export enum GestureGranularity {
  CHARACTER = 0,
  WORD = 1,
  LINE = 2,
  HEADING = 3,
  LINK = 4,
  FORM_FIELD_CONTROL = 5,
  COUNT = 6,
}
