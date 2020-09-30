// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const AutomationNode = chrome.automation.AutomationNode;
const SwitchAccessMenuAction =
    chrome.accessibilityPrivate.SwitchAccessMenuAction;

/** Constants used in Switch Access */
const SAConstants = {

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
    CLOSE_MENU: 1,
    RELOAD_MAIN_MENU: 2,
    OPEN_TEXT_NAVIGATION_MENU: 3,
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
   * IDs of menus that can appear in the menu panel.
   * This must be kept in sync with the div ID of each menu
   * in menu_panel.html.
   * @enum {string}
   * @const
   */
  MenuId: {MAIN: 'main_menu', TEXT_NAVIGATION: 'text_navigation_menu'},

  /**
   * Preferences that are configurable in Switch Access.
   * @enum {string}
   */
  Preference: {
    AUTO_SCAN_ENABLED: 'settings.a11y.switch_access.auto_scan.enabled',
    AUTO_SCAN_TIME: 'settings.a11y.switch_access.auto_scan.speed_ms',
    AUTO_SCAN_KEYBOARD_TIME:
        'settings.a11y.switch_access.auto_scan.keyboard.speed_ms',
    NEXT_SETTING: 'settings.a11y.switch_access.next.setting',
    PREVIOUS_SETTING: 'settings.a11y.switch_access.previous.setting',
    SELECT_SETTING: 'settings.a11y.switch_access.select.setting',
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
      // The ID for the area where text is being input.
      TEXT: 'text'
    },

    /**
     * The inner color of the focus rings.
     * @type {string}
     * @const
     */
    PRIMARY_COLOR: '#8AB4F8',

    /**
     * The outer color of the focus rings.
     * @type {string}
     * @const
     */
    SECONDARY_COLOR: '#000',
  },
};
