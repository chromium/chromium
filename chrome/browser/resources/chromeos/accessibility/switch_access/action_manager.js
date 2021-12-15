// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ActionRecorder} from './action_recorder.js';
import {FocusRingManager} from './focus_ring_manager.js';
import {MenuManager} from './menu_manager.js';
import {SwitchAccessMetrics} from './metrics.js';
import {Navigator} from './navigator.js';
import {SAChildNode, SARootNode} from './nodes/switch_access_node.js';
import {SwitchAccess} from './switch_access.js';
import {SAConstants, SwitchAccessMenuAction} from './switch_access_constants.js';

/**
 * Class to handle performing actions with Switch Access, including determining
 * which actions are available in the given context.
 */
export class ActionManager {
  /** @private */
  constructor() {
    /**
     * The node on which actions are currently being performed.
     * Null if the menu is closed.
     * @private {SAChildNode}
     */
    this.actionNode_;

    /** @private {!Array<!SAConstants.MenuType>} */
    this.menuStack_ = [];
  }

  static get instance() {
    if (!ActionManager.instance_) {
      ActionManager.instance_ = new ActionManager();
    }
    return ActionManager.instance_;
  }

  // ================= Static Methods ==================

  /**
   * Exits all of the open menus and unconditionally closes the menu window.
   */
  static exitAllMenus() {
    ActionManager.instance.menuStack_ = [];
    ActionManager.instance.actionNode_ = null;
    MenuManager.close();
    if (SwitchAccess.mode === SAConstants.Mode.POINT_SCAN) {
      Navigator.byPoint.start();
    } else {
      Navigator.byPoint.stop();
    }
  }

  /**
   * Exits the current menu. If there are no menus on the stack, closes the
   * menu.
   */
  static exitCurrentMenu() {
    ActionManager.instance.menuStack_.pop();
    if (ActionManager.instance.menuStack_.length > 0) {
      ActionManager.instance.openCurrentMenu_();
    } else {
      ActionManager.exitAllMenus();
    }
  }

  /**
   * Handles what to do when the user presses 'select'.
   * If multiple actions are available for the currently highlighted node,
   * opens the action menu. Otherwise performs the node's default action.
   */
  static onSelect() {
    const node = Navigator.byItem.currentNode;
    if (SwitchAccess.instance.multistepAutomationFeaturesEnabled()) {
      ActionRecorder.instance.recordNode(node.automationNode);
    }

    if (MenuManager.isMenuOpen() || node.actions.length <= 1 ||
        !node.location) {
      node.doDefaultAction();
      return;
    }

    ActionManager.instance.menuStack_ = [];
    ActionManager.instance.menuStack_.push(SAConstants.MenuType.MAIN_MENU);
    ActionManager.instance.actionNode_ = node;
    ActionManager.instance.openCurrentMenu_();
  }

  /** @param {!SAConstants.MenuType} menu */
  static openMenu(menu) {
    ActionManager.instance.menuStack_.push(menu);
    ActionManager.instance.openCurrentMenu_();
  }

  /**
   * Given the action to be performed, appropriately handles performing it.
   * @param {!SwitchAccessMenuAction} action
   */
  static performAction(action) {
    SwitchAccessMetrics.recordMenuAction(action);

    // If feature flag is enabled, perform action and escape if successful.
    if (SwitchAccess.instance.multistepAutomationFeaturesEnabled()) {
      if (ActionManager.performActionMultistep(action)) {
        return;
      }
    }

    switch (action) {
      // Global actions:
      case SwitchAccessMenuAction.SETTINGS:
        chrome.accessibilityPrivate.openSettingsSubpage(
            'manageAccessibility/switchAccess');
        ActionManager.exitCurrentMenu();
        break;
      case SwitchAccessMenuAction.POINT_SCAN:
        ActionManager.exitCurrentMenu();
        Navigator.byPoint.start();
        break;
      case SwitchAccessMenuAction.ITEM_SCAN:
        Navigator.byItem.restart();
        ActionManager.exitAllMenus();
        break;
      // Point scan actions:
      case SwitchAccessMenuAction.LEFT_CLICK:
      case SwitchAccessMenuAction.RIGHT_CLICK:
        // Exit menu, then click (so the action will hit the desired target,
        // instead of the menu).
        FocusRingManager.clearAll();
        ActionManager.exitCurrentMenu();
        Navigator.byPoint.performMouseAction(action);
        break;
      // Item scan actions:
      default:
        ActionManager.instance.performActionOnCurrentNode_(action);
    }
  }

  /**
   * Helper method to perform an action when the multistep automation
   * feature flag is enabled.
   * @param {!SwitchAccessMenuAction} action
   * @return {boolean}
   */
  static performActionMultistep(action) {
    // Check feature flag is enabled or escape.
    if (!SwitchAccess.instance.multistepAutomationFeaturesEnabled()) {
      return false;
    }

    switch (action) {
      case SwitchAccessMenuAction.SHORTCUTS:
        ActionManager.openMenu(SAConstants.MenuType.SHORTCUTS_MENU);
        break;
      case SwitchAccessMenuAction.LEAVE_GROUP:
        ActionManager.exitAllMenus();
        Navigator.byItem.exitGroupUnconditionally();
        break;
      case SwitchAccessMenuAction.WEB_MENU:
        ActionManager.openMenu(SAConstants.MenuType.WEB_MENU);
        break;
      case SwitchAccessMenuAction.SYSTEM_MENU:
        ActionManager.openMenu(SAConstants.MenuType.SYSTEM_MENU);
        break;
      case SwitchAccessMenuAction.MEDIA_MENU:
        ActionManager.openMenu(SAConstants.MenuType.MEDIA_MENU);
        break;
      case SwitchAccessMenuAction.DISPLAY_MENU:
        ActionManager.openMenu(SAConstants.MenuType.DISPLAY_MENU);
        break;
      case SwitchAccessMenuAction.USER_MENU:
        ActionManager.openMenu(SAConstants.MenuType.USER_MENU);
        break;
      case SwitchAccessMenuAction.WEB_BOOKMARK:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.D, {ctrl: true});
        break;
      case SwitchAccessMenuAction.WEB_BOTTOM_OF_PAGE:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.RIGHT, {search: true});
        break;
      case SwitchAccessMenuAction.WEB_TOP_OF_PAGE:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.LEFT, {search: true});
        break;
      case SwitchAccessMenuAction.WEB_FIND_IN_PAGE:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.F, {ctrl: true});
        break;
      case SwitchAccessMenuAction.WEB_DOWNLOADS:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.J, {ctrl: true});
        break;
      case SwitchAccessMenuAction.WEB_CLEAR_HISTORY:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.BACK, {ctrl: true, shift: true});
        break;
      case SwitchAccessMenuAction.SYSTEM_STATUS_BAR:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.S, {alt: true, shift: true});
        break;
      case SwitchAccessMenuAction.SYSTEM_LAUNCHER:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.SEARCH);
        break;
      case SwitchAccessMenuAction.SYSTEM_TASK_MANAGER:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.ESCAPE, {search: true});
        break;
      case SwitchAccessMenuAction.SYSTEM_DIAGNOSTICS:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.ESCAPE, {ctrl: true, search: true});
        break;
      case SwitchAccessMenuAction.SYSTEM_SCREENSHOT:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.MEDIA_LAUNCH_APP1, {ctrl: true});
        break;
      case SwitchAccessMenuAction.SYSTEM_HELP:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.OEM_2, {ctrl: true});
        break;
      case SwitchAccessMenuAction.MEDIA_MUTE:
        EventGenerator.sendKeyPress(KeyCode.VOLUME_MUTE);
        break;
      case SwitchAccessMenuAction.MEDIA_VOLUME_DOWN:
        EventGenerator.sendKeyPress(KeyCode.VOLUME_DOWN);
        break;
      case SwitchAccessMenuAction.MEDIA_VOLUME_UP:
        EventGenerator.sendKeyPress(KeyCode.VOLUME_UP);
        break;
      case SwitchAccessMenuAction.MEDIA_REWIND:
        // TODO(crbug.com/1258921): Fill in rewind or remove.
        break;
      case SwitchAccessMenuAction.MEDIA_PLAY_PAUSE:
        EventGenerator.sendKeyPress(KeyCode.MEDIA_PLAY_PAUSE);
        break;
      case SwitchAccessMenuAction.MEDIA_FASTFORWARD:
        // TODO(crbug.com/1258921): Fill in fastforward or remove.
        break;
      case SwitchAccessMenuAction.DISPLAY_MIRROR:
        EventGenerator.sendKeyPress(KeyCode.ZOOM, {ctrl: true});
        break;
      case SwitchAccessMenuAction.DISPLAY_BRIGHTNESS_DOWN:
        EventGenerator.sendKeyPress(KeyCode.BRIGHTNESS_DOWN);
        break;
      case SwitchAccessMenuAction.DISPLAY_BRIGHTNESS_UP:
        EventGenerator.sendKeyPress(KeyCode.BRIGHTNESS_UP);
        break;
      case SwitchAccessMenuAction.DISPLAY_ROTATE:
        EventGenerator.sendKeyPress(
            KeyCode.BROWSER_REFRESH, {ctrl: true, alt: true, shift: true});
        break;
      case SwitchAccessMenuAction.DISPLAY_ZOOM_OUT:
        EventGenerator.sendKeyPress(KeyCode.OEM_MINUS, {ctrl: true});
        break;
      case SwitchAccessMenuAction.DISPLAY_ZOOM_IN:
        EventGenerator.sendKeyPress(KeyCode.OEM_PLUS, {ctrl: true});
        break;
      case SwitchAccessMenuAction.USER_LOCK:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.L, {search: true});
        break;
      case SwitchAccessMenuAction.USER_PREVIOUS_USER:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.OEM_COMMA, {ctrl: true, alt: true});
        break;
      case SwitchAccessMenuAction.USER_NEXT_USER:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(
            KeyCode.OEM_PERIOD, {ctrl: true, alt: true});
        break;
      case SwitchAccessMenuAction.USER_SIGN_OUT:
        FocusRingManager.clearAll();
        ActionManager.exitAllMenus();
        EventGenerator.sendKeyPress(KeyCode.Q, {ctrl: true, shift: true});
        EventGenerator.sendKeyPress(KeyCode.Q, {ctrl: true, shift: true});
        break;
      case SwitchAccessMenuAction.ACTION_RECORDER:
        ActionManager.openMenu(SAConstants.MenuType.ACTION_RECORDER_MENU);
        break;
      case SwitchAccessMenuAction.START_RECORDING:
        ActionManager.exitAllMenus();
        ActionRecorder.instance.start();
        break;
      case SwitchAccessMenuAction.STOP_RECORDING:
        ActionRecorder.instance.stop();
        break;
      case SwitchAccessMenuAction.EXECUTE_MACRO:
        ActionRecorder.instance.executeMacro();
        break;
      default:
        return false;
    }

    return true;
  }

  /** Refreshes the current menu, if needed. */
  static refreshMenuUnconditionally() {
    if (!MenuManager.isMenuOpen()) {
      return;
    }

    ActionManager.instance.openCurrentMenu_();
  }

  /**
   * Refreshes the current menu, if the current action node matches the node
   * provided.
   * @param {!SAChildNode} node
   */
  static refreshMenuForNode(node) {
    if (node.equals(ActionManager.instance.actionNode_)) {
      ActionManager.refreshMenuUnconditionally();
    }
  }

  // ================= Private Methods ==================

  /**
   * Returns all possible actions for the provided menu type
   * @param {!SAConstants.MenuType} type
   * @return {!Array<!SwitchAccessMenuAction>}
   * @private
   */
  actionsForType_(type) {
    // If feature flag is enabled, fill submenus and escape if successful.
    if (SwitchAccess.instance.multistepAutomationFeaturesEnabled()) {
      const actions = this.actionsForTypeMultistep_(type);
      if (actions.length) {
        return actions;
      }
    }

    switch (type) {
      case SAConstants.MenuType.MAIN_MENU:
        return [
          SwitchAccessMenuAction.COPY,
          SwitchAccessMenuAction.CUT,
          SwitchAccessMenuAction.DECREMENT,
          SwitchAccessMenuAction.DICTATION,
          SwitchAccessMenuAction.INCREMENT,
          SwitchAccessMenuAction.KEYBOARD,
          SwitchAccessMenuAction.MOVE_CURSOR,
          SwitchAccessMenuAction.PASTE,
          SwitchAccessMenuAction.SCROLL_DOWN,
          SwitchAccessMenuAction.SCROLL_LEFT,
          SwitchAccessMenuAction.SCROLL_RIGHT,
          SwitchAccessMenuAction.SCROLL_UP,
          SwitchAccessMenuAction.SELECT,
          SwitchAccessMenuAction.START_TEXT_SELECTION,
        ];

      case SAConstants.MenuType.TEXT_NAVIGATION:
        return [
          SwitchAccessMenuAction.JUMP_TO_BEGINNING_OF_TEXT,
          SwitchAccessMenuAction.JUMP_TO_END_OF_TEXT,
          SwitchAccessMenuAction.MOVE_UP_ONE_LINE_OF_TEXT,
          SwitchAccessMenuAction.MOVE_DOWN_ONE_LINE_OF_TEXT,
          SwitchAccessMenuAction.MOVE_BACKWARD_ONE_WORD_OF_TEXT,
          SwitchAccessMenuAction.MOVE_FORWARD_ONE_WORD_OF_TEXT,
          SwitchAccessMenuAction.MOVE_BACKWARD_ONE_CHAR_OF_TEXT,
          SwitchAccessMenuAction.MOVE_FORWARD_ONE_CHAR_OF_TEXT,
          SwitchAccessMenuAction.END_TEXT_SELECTION
        ];
      case SAConstants.MenuType.POINT_SCAN_MENU:
        return [
          SwitchAccessMenuAction.LEFT_CLICK,
          SwitchAccessMenuAction.RIGHT_CLICK,
        ];

      default:
        return [];
    }
  }

  /**
   * Returns all possible actions for the provided menu type when the multistep
   * automation feature flag is enabled.
   * @param {!SAConstants.MenuType} type
   * @return {!Array<!SwitchAccessMenuAction>}
   * @private
   */
  actionsForTypeMultistep_(type) {
    // Check feature flag is enabled or escape.
    if (!SwitchAccess.instance.multistepAutomationFeaturesEnabled()) {
      return [];
    }

    switch (type) {
      case SAConstants.MenuType.SHORTCUTS_MENU:
        return [
          SwitchAccessMenuAction.LEAVE_GROUP, SwitchAccessMenuAction.WEB_MENU,
          SwitchAccessMenuAction.SYSTEM_MENU, SwitchAccessMenuAction.MEDIA_MENU,
          SwitchAccessMenuAction.DISPLAY_MENU, SwitchAccessMenuAction.USER_MENU
        ];
      case SAConstants.MenuType.WEB_MENU:
        return [
          SwitchAccessMenuAction.WEB_BOOKMARK,
          SwitchAccessMenuAction.WEB_BOTTOM_OF_PAGE,
          SwitchAccessMenuAction.WEB_TOP_OF_PAGE,
          SwitchAccessMenuAction.WEB_FIND_IN_PAGE,
          SwitchAccessMenuAction.WEB_DOWNLOADS,
          SwitchAccessMenuAction.WEB_CLEAR_HISTORY
        ];
      case SAConstants.MenuType.SYSTEM_MENU:
        return [
          SwitchAccessMenuAction.SYSTEM_STATUS_BAR,
          SwitchAccessMenuAction.SYSTEM_LAUNCHER,
          SwitchAccessMenuAction.SYSTEM_TASK_MANAGER,
          SwitchAccessMenuAction.SYSTEM_DIAGNOSTICS,
          SwitchAccessMenuAction.SYSTEM_SCREENSHOT,
          SwitchAccessMenuAction.SYSTEM_HELP
        ];
      case SAConstants.MenuType.MEDIA_MENU:
        return [
          SwitchAccessMenuAction.MEDIA_MUTE,
          SwitchAccessMenuAction.MEDIA_VOLUME_DOWN,
          SwitchAccessMenuAction.MEDIA_VOLUME_UP,
          SwitchAccessMenuAction.MEDIA_REWIND,
          SwitchAccessMenuAction.MEDIA_PLAY_PAUSE,
          SwitchAccessMenuAction.MEDIA_FASTFORWARD
        ];
      case SAConstants.MenuType.DISPLAY_MENU:
        return [
          SwitchAccessMenuAction.DISPLAY_MIRROR,
          SwitchAccessMenuAction.DISPLAY_BRIGHTNESS_DOWN,
          SwitchAccessMenuAction.DISPLAY_BRIGHTNESS_UP,
          SwitchAccessMenuAction.DISPLAY_ROTATE,
          SwitchAccessMenuAction.DISPLAY_ZOOM_OUT,
          SwitchAccessMenuAction.DISPLAY_ZOOM_IN
        ];
      case SAConstants.MenuType.USER_MENU:
        return [
          SwitchAccessMenuAction.USER_LOCK,
          SwitchAccessMenuAction.USER_PREVIOUS_USER,
          SwitchAccessMenuAction.USER_NEXT_USER,
          SwitchAccessMenuAction.USER_SIGN_OUT
        ];
      case SAConstants.MenuType.ACTION_RECORDER_MENU:
        return [
          SwitchAccessMenuAction.START_RECORDING,
          SwitchAccessMenuAction.STOP_RECORDING,
          SwitchAccessMenuAction.EXECUTE_MACRO,
        ];
      default:
        return [];
    }
  }

  /**
   * @param {!Array<!SwitchAccessMenuAction>} actions
   * @return {!Array<!SwitchAccessMenuAction>}
   * @private
   */
  addGlobalActions_(actions) {
    if (SwitchAccess.mode === SAConstants.Mode.POINT_SCAN) {
      actions.push(SwitchAccessMenuAction.ITEM_SCAN);
    } else {
      actions.push(SwitchAccessMenuAction.POINT_SCAN);
    }
    actions.push(SwitchAccessMenuAction.SETTINGS);
    return actions;
  }

  /**
   * @return {!SAConstants.MenuType}
   * @private
   */
  get currentMenuType_() {
    return this.menuStack_[this.menuStack_.length - 1];
  }

  /**
   * @return {!Array<!SwitchAccessMenuAction>}
   * @private
   */
  getActionsForCurrentMenuAndNode_() {
    if (this.currentMenuType_ === SAConstants.MenuType.POINT_SCAN_MENU) {
      let actions = this.actionsForType_(SAConstants.MenuType.POINT_SCAN_MENU);
      actions = this.addGlobalActions_(actions);
      return actions;
    } else if (this.currentMenuType_ === SAConstants.MenuType.SHORTCUTS_MENU) {
      return this.actionsForType_(SAConstants.MenuType.SHORTCUTS_MENU);
    } else if (this.currentMenuType_ === SAConstants.MenuType.WEB_MENU) {
      return this.actionsForType_(SAConstants.MenuType.WEB_MENU);
    } else if (this.currentMenuType_ === SAConstants.MenuType.SYSTEM_MENU) {
      return this.actionsForType_(SAConstants.MenuType.SYSTEM_MENU);
    } else if (this.currentMenuType_ === SAConstants.MenuType.MEDIA_MENU) {
      return this.actionsForType_(SAConstants.MenuType.MEDIA_MENU);
    } else if (this.currentMenuType_ === SAConstants.MenuType.DISPLAY_MENU) {
      return this.actionsForType_(SAConstants.MenuType.DISPLAY_MENU);
    } else if (this.currentMenuType_ === SAConstants.MenuType.USER_MENU) {
      return this.actionsForType_(SAConstants.MenuType.USER_MENU);
    } else if (
        this.currentMenuType_ === SAConstants.MenuType.ACTION_RECORDER_MENU) {
      return this.actionsForType_(SAConstants.MenuType.ACTION_RECORDER_MENU);
    }


    if (!this.actionNode_ || !this.actionNode_.isValidAndVisible()) {
      return [];
    }
    let actions = this.actionNode_.actions;
    const possibleActions = this.actionsForType_(this.currentMenuType_);
    actions = actions.filter((a) => possibleActions.includes(a));
    if (this.currentMenuType_ === SAConstants.MenuType.MAIN_MENU) {
      actions = this.addGlobalActions_(actions);
      if (SwitchAccess.instance.multistepAutomationFeaturesEnabled()) {
        // Ensure shortcuts and action recorder are the first items in the
        // menu.
        actions.unshift(SwitchAccessMenuAction.ACTION_RECORDER);
        actions.unshift(SwitchAccessMenuAction.SHORTCUTS);
      }
    }
    return actions;
  }

  /**
   * @return {chrome.accessibilityPrivate.ScreenRect|undefined}
   * @private
   */
  getLocationForCurrentMenuAndNode_() {
    if (this.currentMenuType_ === SAConstants.MenuType.POINT_SCAN_MENU) {
      return {
        left: Math.floor(Navigator.byPoint.currentPoint.x),
        top: Math.floor(Navigator.byPoint.currentPoint.y),
        width: 1,
        height: 1
      };
    }

    if (this.actionNode_) {
      return this.actionNode_.location;
    }

    return undefined;
  }

  /** @private */
  openCurrentMenu_() {
    const actions = this.getActionsForCurrentMenuAndNode_();
    const location = this.getLocationForCurrentMenuAndNode_();

    if (actions.length < 2) {
      ActionManager.exitCurrentMenu();
    }
    MenuManager.open(actions, location);
  }

  /**
   * @param {!SwitchAccessMenuAction} action
   * @private
   */
  performActionOnCurrentNode_(action) {
    if (!this.actionNode_.hasAction(action)) {
      ActionManager.refreshMenuUnconditionally();
      return;
    }

    // We exit the menu before asking the node to perform the action, because
    // having the menu on the group stack interferes with some actions. We do
    // not close the menu bubble until we receive the ActionResponse CLOSE_MENU.
    // If we receive a different response, we re-enter the menu.
    Navigator.byItem.suspendCurrentGroup();

    const response = this.actionNode_.performAction(action);

    switch (response) {
      case SAConstants.ActionResponse.CLOSE_MENU:
        ActionManager.exitAllMenus();
        return;
      case SAConstants.ActionResponse.EXIT_SUBMENU:
        ActionManager.exitCurrentMenu();
        return;
      case SAConstants.ActionResponse.REMAIN_OPEN:
        Navigator.byItem.restoreSuspendedGroup();
        return;
      case SAConstants.ActionResponse.RELOAD_MENU:
        ActionManager.refreshMenuUnconditionally();
        return;
      case SAConstants.ActionResponse.OPEN_TEXT_NAVIGATION_MENU:
        if (SwitchAccess.instance.improvedTextInputEnabled()) {
          this.menuStack_.push(SAConstants.MenuType.TEXT_NAVIGATION);
        }
        this.openCurrentMenu_();
    }
  }
}
