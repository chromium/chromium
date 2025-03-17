// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Constants used in Switch Access.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

/** When an action is performed, how the menu should respond. */
export enum ActionResponse {
  NO_ACTION_TAKEN = -1,
  REMAIN_OPEN,
  CLOSE_MENU,
  EXIT_SUBMENU,
  RELOAD_MENU,
  OPEN_TEXT_NAVIGATION_MENU,
}

/**
 * The types of error or unexpected state that can be encountered by Switch
 * Access.
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 */
export enum ErrorType {
  UNKNOWN = 0,
  PREFERENCE_TYPE = 1,
  UNTRANSLATED_STRING = 2,
  INVALID_COLOR = 3,
  NEXT_UNDEFINED = 4,
  PREVIOUS_UNDEFINED = 5,
  NULL_CHILD = 6,
  NO_CHILDREN = 7,
  MALFORMED_DESKTOP = 8,
  MISSING_LOCATION = 9,
  MISSING_KEYBOARD = 10,
  ROW_TOO_SHORT = 11,
  MISSING_BASE_NODE = 12,
  NEXT_INVALID = 13,
  PREVIOUS_INVALID = 14,
  INVALID_SELECTION_BOUNDS = 15,
  UNINITIALIZED = 16,
  DUPLICATE_INITIALIZATION = 17,
}

/** The different types of menus and sub-menus that can be shown. */
export enum MenuType {
  MAIN_MENU,
  TEXT_NAVIGATION,
  POINT_SCAN_MENU,
}

/**
 * The modes of interaction the user can select for how to interact with the
 * device.
 */
export enum Mode {
  ITEM_SCAN,
  POINT_SCAN,
}

TestImportManager.exportForTesting(['Mode', Mode]);
