// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const AutomationNode = chrome.automation.AutomationNode;
export const SwitchAccessMenuAction =
    chrome.accessibilityPrivate.SwitchAccessMenuAction;

/** Constants used in Switch Access */
export const SAConstants = {

  // ========================= Constants =========================

  /**
   * The ID of the back button.
   * This must be kept in sync with the back button ID in menu_panel.html.
   * @type {string}
   * @const
   */
  BACK_ID: 'back',

  /**
   * The ID of the menu panel.
   * This must be kept in sync with the div ID in menu_panel.html.
   * @type {string}
   * @const
   */
  MENU_PANEL_ID: 'switchaccess_menu_actions',

  /**
   * The delay between keydown and keyup events on the virtual keyboard,
   * allowing the key press animation to display.
   * @type {number}
   * @const
   */
  VK_KEY_PRESS_DURATION_MS: 100,

  // =========================== Enums ===========================

  /**
   * When an action is performed, how the menu should respond.
   * @enum {number}
   */
  ActionResponse: {
    NO_ACTION_TAKEN: -1,
    REMAIN_OPEN: 0,
    // Closes the menu entirely.
    CLOSE_MENU: 1,
    // Exits the current submenu.
    EXIT_SUBMENU: 2,
    RELOAD_MENU: 3,
    OPEN_TEXT_NAVIGATION_MENU: 4,
  },

  /**
   * The types of error or unexpected state that can be encountered by Switch
   * Access.
   * These values are persisted to logs and should not be renumbered or re-used.
   * See tools/metrics/histograms/enums.xml.
   * @enum {number}
   * @const
   */
  ErrorType: {
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
  },

  /**
   * The different types of menus and sub-menus that can be shown.
   * @enum {number}
   * @const
   */
  MenuType: {MAIN_MENU: 0, TEXT_NAVIGATION: 1, POINT_SCAN_MENU: 2},

  /**
   * The modes of interaction the user can select for how to interact with the
   * device.
   * @enum {number}
   * @const
   */
  Mode: {ITEM_SCAN: 0, POINT_SCAN: 1},

  /**
   * Preferences that are configurable in Switch Access.
   * @enum {string}
   */
  Preference: {
    AUTO_SCAN_ENABLED: 'settings.a11y.switch_access.auto_scan.enabled',
    AUTO_SCAN_TIME: 'settings.a11y.switch_access.auto_scan.speed_ms',
    AUTO_SCAN_KEYBOARD_TIME:
        'settings.a11y.switch_access.auto_scan.keyboard.speed_ms',
    NEXT_DEVICE_KEY_CODES: 'settings.a11y.switch_access.next.device_key_codes',
    PREVIOUS_DEVICE_KEY_CODES:
        'settings.a11y.switch_access.previous.device_key_codes',
    SELECT_DEVICE_KEY_CODES:
        'settings.a11y.switch_access.select.device_key_codes',
  },

  // =========================== Sub-objects ===========================

  Focus: {
    /**
     * The amount of space (in px) needed to fit a focus ring around an element.
     * @type {number}
     * @const
     */
    BUFFER: 4,

    /**
     * The name of the focus class for the menu.
     * This must be kept in sync with the class name in menu_panel.css.
     * @type {string}
     * @const
     */
    CLASS: 'focus',

    /**
     * The buffer (in dip) between a child's focus ring and its parent's focus
     * ring.
     * @type {number}
     * @const
     */
    GROUP_BUFFER: 2,

    /**
     * The focus ring IDs used by Switch Access.
     * @enum {string}
     */
    ID: {
      // The ID for the ring showing the user's current focus.
      PRIMARY: 'primary',
      // The ID for the ring showing a preview of the next focus, if the user
      // selects the current element.
      PREVIEW: 'preview',
    },

    /**
     * The inner color of the primary focus ring.
     * @type {string}
     * @const
     */
    PRIMARY_COLOR: '#8AB4F8',  // Google Blue 300

    /**
     * The inner color of the preview focus ring
     * @type {string}
     * @const
     */
    PREVIEW_COLOR: '#8AB4F880',  // Google Blue 300, 50% opacity

    /**
     * The outer color of the focus rings.
     * @type {string}
     * @const
     */
    OUTER_COLOR: '#174EA6',  // Google Blue 900
  },
};
