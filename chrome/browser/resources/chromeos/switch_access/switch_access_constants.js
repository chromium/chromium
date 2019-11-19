// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Constants used in Switch Access */
let SAConstants = {};
SAConstants.Focus = {};

// ========================= Constants =========================

/**
 * The ID of the back button.
 * This must be kept in sync with the back button ID in menu_panel.html.
 * @type {string}
 * @const
 */
SAConstants.BACK_ID = 'back';

/**
 * The amount of space (in px) needed to fit a focus ring around an element.
 * @type {number}
 * @const
 */
SAConstants.Focus.BUFFER = 4;

/**
 * The name of the focus class for the menu.
 * This must be kept in sync with the class name in menu_panel.css.
 * @type {string}
 * @const
 */
SAConstants.Focus.CLASS = 'focus';

/**
 * The buffer (in dip) between a child's focus ring and its parent's focus ring.
 * @type {number}
 * @const
 */
SAConstants.Focus.GROUP_BUFFER = 2;

/**
 * The inner color of the focus rings.
 * @type {string}
 * @const
 */
SAConstants.Focus.PRIMARY_COLOR = '#1A73E8FF';

/**
 * The outer color of the focus rings.
 * @type {string}
 * @const
 */
SAConstants.Focus.SECONDARY_COLOR = '#0006';

/**
 * The maximum length of a row in the Virtual Keyboard.
 * @type {number}
 * @const
 */
SAConstants.KEYBOARD_MAX_ROW_LENGTH = 14;

/**
 * The ID of the menu panel.
 * This must be kept in sync with the div ID in menu_panel.html.
 * @type {string}
 * @const
 */
SAConstants.MENU_PANEL_ID = 'switchaccess_menu_actions';

/**
 * The delay between keydown and keyup events on the virtual keyboard,
 * allowing the key press animation to display.
 * @type {number}
 * @const
 */
SAConstants.VK_KEY_PRESS_DURATION_MS = 100;

// =========================== Enums ===========================

/**
 * The focus ring IDs used by Switch Access.
 * @enum {string}
 */
SAConstants.Focus.ID = {
  // The ID for the ring showing the user's current focus.
  PRIMARY: 'primary',
  // The ID for the ring showing the next focus.
  NEXT: 'next',
  // The ID for the area where text is being input.
  TEXT: 'text'
};

/**
 * Actions available in the Switch Access Menu.
 * @enum {string}
 * @const
 */
SAConstants.MenuAction = {
  // Copy text.
  COPY: 'copy',
  // Cut text.
  CUT: 'cut',
  // Decrement the value of an input field.
  DECREMENT: chrome.automation.ActionType.DECREMENT,
  // Activate dictation for voice input to an editable text region.
  DICTATION: 'dictation',
  // Increment the value of an input field.
  INCREMENT: chrome.automation.ActionType.INCREMENT,
  // Move text caret to the beginning of the text field.
  JUMP_TO_BEGINNING_OF_TEXT: 'jumpToBeginningOfText',
  // Move text caret to the end of the text field.
  JUMP_TO_END_OF_TEXT: 'jumpToEndOfText',
  // Open and jump to the virtual keyboard
  OPEN_KEYBOARD: 'keyboard',
  // Move text caret one character backward.
  MOVE_BACKWARD_ONE_CHAR_OF_TEXT: 'moveBackwardOneCharOfText',
  // Move text caret one word backward.
  MOVE_BACKWARD_ONE_WORD_OF_TEXT: 'moveBackwardOneWordOfText',
  // Open the text navigation menu to move the text caret.
  MOVE_CURSOR: 'moveCursor',
  // Move text caret one line down.
  MOVE_DOWN_ONE_LINE_OF_TEXT: 'moveDownOneLineOfText',
  // Move text caret one character forward.
  MOVE_FORWARD_ONE_CHAR_OF_TEXT: 'moveForwardOneCharOfText',
  // Move text caret one word forward.
  MOVE_FORWARD_ONE_WORD_OF_TEXT: 'moveForwardOneWordOfText',
  // Move text caret one line up.
  MOVE_UP_ONE_LINE_OF_TEXT: 'moveUpOneLineOfText',
  // Paste text.
  PASTE: 'paste',
  // Scroll the current element (or its ancestor) logically backwards.
  // Primarily used by ARC++ apps.
  SCROLL_BACKWARD: chrome.automation.ActionType.SCROLL_BACKWARD,
  // Scroll the current element (or its ancestor) down.
  SCROLL_DOWN: chrome.automation.ActionType.SCROLL_DOWN,
  // Scroll the current element (or is ancestor) logically forwards.
  // Primarily used by ARC++ apps.
  SCROLL_FORWARD: chrome.automation.ActionType.SCROLL_FORWARD,
  // Scroll the current element (or its ancestor) left.
  SCROLL_LEFT: chrome.automation.ActionType.SCROLL_LEFT,
  // Scroll the current element (or its ancestor) right.
  SCROLL_RIGHT: chrome.automation.ActionType.SCROLL_RIGHT,
  // Scroll the current element (or its ancestor) up.
  SCROLL_UP: chrome.automation.ActionType.SCROLL_UP,
  // Either perform the default action or enter a new group, as applicable.
  SELECT: 'select',
  // Open and jump to the Switch Access settings.
  SETTINGS: 'settings',
  // Set the end of a text selection.
  SELECT_END: 'selectEnd',
  // Set the beginning of a text selection.
  SELECT_START: 'selectStart'
};

/**
 * IDs of menus that can appear in the menu panel.
 * This must be kept in sync with the div ID of each menu
 * in menu_panel.html.
 * @enum {string}
 * @const
 */
SAConstants.MenuId = {
  MAIN: 'main_menu',
  TEXT_NAVIGATION: 'text_navigation_menu'
};

/**
 * Preferences that are configurable in Switch Access.
 * @enum {string}
 */
SAConstants.Preference = {
  AUTO_SCAN_ENABLED: 'settings.a11y.switch_access.auto_scan.enabled',
  AUTO_SCAN_TIME: 'settings.a11y.switch_access.auto_scan.speed_ms',
  AUTO_SCAN_KEYBOARD_TIME:
      'settings.a11y.switch_access.auto_scan.keyboard.speed_ms',
  NEXT_SETTING: 'settings.a11y.switch_access.next.setting',
  PREVIOUS_SETTING: 'settings.a11y.switch_access.previous.setting',
  SELECT_SETTING: 'settings.a11y.switch_access.select.setting',
};

/**
 * The types of error or unexpected state that can be encountered by Switch
 * Access.
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 * @const
 */
SAConstants.ErrorType = {
  UNKNOWN: 0,
  PREFERENCE_TYPE: 1,
  UNTRANSLATED_STRING: 2,
  INVALID_COLOR: 3,
  NEXT_UNDEFINED: 4,
  PREVIOUS_UNDEFINED: 5,
  NULL_CHILD: 6,
  NO_CHILDREN: 7,
  MALFORMED_DESKTOP: 8,
  MISSING_LOCATION: 9,
};
