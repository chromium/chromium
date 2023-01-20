// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Constants used in Switch Access.
 */
const AutomationNode = chrome.automation.AutomationNode;

/**
 * When an action is performed, how the menu should respond.
 * @enum {number}
 */
export const ActionResponse = {
  NO_ACTION_TAKEN: -1,
  REMAIN_OPEN: 0,
  CLOSE_MENU: 1,
  EXIT_SUBMENU: 2,
  RELOAD_MENU: 3,
  OPEN_TEXT_NAVIGATION_MENU: 4,
};

/**
 * The types of error or unexpected state that can be encountered by Switch
 * Access.
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const ErrorType = {
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
  MISSING_KEYBOARD: 10,
  ROW_TOO_SHORT: 11,
  MISSING_BASE_NODE: 12,
  NEXT_INVALID: 13,
  PREVIOUS_INVALID: 14,
  INVALID_SELECTION_BOUNDS: 15,
  UNINITIALIZED: 16,
};

/**
 * The different types of menus and sub-menus that can be shown.
 * @enum {number}
 */
export const MenuType = {
  MAIN_MENU: 0,
  TEXT_NAVIGATION: 1,
  POINT_SCAN_MENU: 2,
};

/**
 * The modes of interaction the user can select for how to interact with the
 * device.
 * @enum {number}
 */
export const Mode = {
  ITEM_SCAN: 0,
  POINT_SCAN: 1,
};
