// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MenuManager} from './menu_manager.js';
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
    if (node.actions.length <= 1 || !node.location) {
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
    const manager = ActionManager.instance;
    manager.handleGlobalActions_(action) ||
        manager.handlePointScanActions_(action) ||
        manager.performActionOnCurrentNode_(action);
    ActionManager.exitCurrentMenu();
  }


  /** Refreshes the current menu, if needed. */
  static refreshMenu() {
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
      ActionManager.refreshMenu();
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
    }

    if (!this.actionNode_ || !this.actionNode_.isValidAndVisible()) {
      return [];
    }
    let actions = this.actionNode_.actions;
    const possibleActions = this.actionsForType_(this.currentMenuType_);
    actions.filter((a) => possibleActions.includes(a));
    if (this.currentMenuType_ === SAConstants.MenuType.MAIN_MENU) {
      actions = this.addGlobalActions_(actions);
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

  /**
   * If the action is a global action, perform the action and return true.
   * Otherwise return false.
   * @param {!SwitchAccessMenuAction} action
   * @return {boolean}
   * @private
   */
  handleGlobalActions_(action) {
    switch (action) {
      case SwitchAccessMenuAction.SETTINGS:
        chrome.accessibilityPrivate.openSettingsSubpage(
            'manageAccessibility/switchAccess');
        return true;
      case SwitchAccessMenuAction.POINT_SCAN:
        ActionManager.exitCurrentMenu();
        Navigator.byPoint.start();
        return true;
      case SwitchAccessMenuAction.ITEM_SCAN:
        Navigator.byPoint.stop();
        return true;
      default:
        return false;
    }
  }

  /**
   * If the action is a point scan action, perform the action and return true.
   * Otherwise return false.
   * @param {!SwitchAccessMenuAction} action
   * @return {boolean}
   * @private
   */
  handlePointScanActions_(action) {
    if (SwitchAccess.mode !== SAConstants.Mode.POINT_SCAN) {
      return false;
    }

    switch (action) {
      case SwitchAccessMenuAction.LEFT_CLICK:
        EventGenerator.sendMouseClick(
            Navigator.byPoint.currentPoint.x, Navigator.byPoint.currentPoint.y);
        Navigator.byPoint.start();
        return true;
      case SwitchAccessMenuAction.RIGHT_CLICK:
        EventGenerator.sendMouseClick(
            Navigator.byPoint.currentPoint.x, Navigator.byPoint.currentPoint.y,
            {
              mouseButton:
                  chrome.accessibilityPrivate.SyntheticMouseEventButton.RIGHT
            });
        Navigator.byPoint.start();
        return true;
      default:
        return false;
    }
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
      // Refresh the actions in the menu.
      this.openCurrentMenu_();
      return;
    }

    // We exit the menu before asking the node to perform the action, because
    // having the menu on the group stack interferes with some actions. We do
    // not close the menu bubble until we receive the ActionResponse CLOSE_MENU.
    // If we receive a different response, we re-enter the menu.
    Navigator.byItem.exitIfInGroup(MenuManager.menuAutomationNode);
    const response = this.actionNode_.performAction(action);
    if (response === SAConstants.ActionResponse.CLOSE_MENU) {
      MenuManager.close();
    } else {
      Navigator.byItem.jumpToSwitchAccessMenu();
    }

    switch (response) {
      case SAConstants.ActionResponse.RELOAD_MENU:
        this.openCurrentMenu_();
        break;
      case SAConstants.ActionResponse.OPEN_TEXT_NAVIGATION_MENU:
        if (SwitchAccess.instance.improvedTextInputEnabled()) {
          this.menuStack_.push(SAConstants.MenuType.TEXT_NAVIGATION);
        }
        this.openCurrentMenu_();
    }
  }
}
