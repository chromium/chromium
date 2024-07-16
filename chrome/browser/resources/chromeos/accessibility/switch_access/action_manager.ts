// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

import {FocusRingManager} from './focus_ring_manager.js';
import {MenuManager} from './menu_manager.js';
import {SwitchAccessMetrics} from './metrics.js';
import {Navigator} from './navigator.js';
import {SAChildNode} from './nodes/switch_access_node.js';
import {SwitchAccess} from './switch_access.js';
import {ActionResponse, ErrorType, MenuType, Mode} from './switch_access_constants.js';

import MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;
import Rect = chrome.automation.Rect;

/**
 * Class to handle performing actions with Switch Access, including determining
 * which actions are available in the given context.
 */
export class ActionManager {
  /**
   * The node on which actions are currently being performed.
   * Null if the menu is closed.
   */
  private actionNode_?: SAChildNode | null;
  private menuManager_: MenuManager;
  private menuStack_: MenuType[] = [];

  static instance?: ActionManager;

  private constructor() {
    this.menuManager_ = MenuManager.create();
  }

  static init(): void {
    if (ActionManager.instance) {
      throw SwitchAccess.error(
          ErrorType.DUPLICATE_INITIALIZATION,
          'Cannot call ActionManager.init() more than once.');
    }
    ActionManager.instance = new ActionManager();
  }

  // ================= Static Methods ==================

  /**
   * Exits all of the open menus and unconditionally closes the menu window.
   */
  static exitAllMenus(): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    ActionManager.instance!.menuStack_ = [];
    ActionManager.instance!.actionNode_ = null;
    ActionManager.instance!.menuManager_.close();
    if (SwitchAccess.mode === Mode.POINT_SCAN) {
      Navigator.byPoint.start();
    } else {
      Navigator.byPoint.stop();
    }
  }

  /**
   * Exits the current menu. If there are no menus on the stack, closes the
   * menu.
   */
  static exitCurrentMenu(): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    ActionManager.instance!.menuStack_.pop();
    if (ActionManager.instance!.menuStack_.length > 0) {
      ActionManager.instance!.openCurrentMenu_();
    } else {
      ActionManager.exitAllMenus();
    }
  }

  /**
   * Handles what to do when the user presses 'select'.
   * If multiple actions are available for the currently highlighted node,
   * opens the action menu. Otherwise performs the node's default action.
   */
  static onSelect(): void {
    const node = Navigator.byItem.currentNode;
    if (MenuManager.isMenuOpen() || node.actions.length <= 1 ||
        !node.location) {
      node.doDefaultAction();
      return;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    ActionManager.instance!.menuStack_ = [];
    ActionManager.instance!.menuStack_.push(MenuType.MAIN_MENU);
    ActionManager.instance!.actionNode_ = node;
    ActionManager.instance!.openCurrentMenu_();
  }

  static openMenu(menu: MenuType): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    ActionManager.instance!.menuStack_.push(menu);
    ActionManager.instance!.openCurrentMenu_();
  }

  /** Given the action to be performed, appropriately handles performing it. */
  static performAction(action: MenuAction): void {
    SwitchAccessMetrics.recordMenuAction(action);

    switch (action) {
      // Global actions:
      case MenuAction.SETTINGS:
        chrome.accessibilityPrivate.openSettingsSubpage(
            'manageAccessibility/switchAccess');
        ActionManager.exitCurrentMenu();
        break;
      case MenuAction.POINT_SCAN:
        ActionManager.exitCurrentMenu();
        Navigator.byPoint.start();
        break;
      case MenuAction.ITEM_SCAN:
        Navigator.byItem.restart();
        ActionManager.exitAllMenus();
        break;
      // Point scan actions:
      case MenuAction.LEFT_CLICK:
      case MenuAction.RIGHT_CLICK:
        // Exit menu, then click (so the action will hit the desired target,
        // instead of the menu).
        FocusRingManager.clearAll();
        ActionManager.exitCurrentMenu();
        Navigator.byPoint.performMouseAction(action);
        break;
      // Item scan actions:
      default:
        // TODO(b/314203187): Not null asserted, check that this is correct.
        ActionManager.instance!.performActionOnCurrentNode_(action);
    }
  }

  /** Refreshes the current menu, if needed. */
  static refreshMenuUnconditionally(): void {
    if (!MenuManager.isMenuOpen()) {
      return;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    ActionManager.instance!.openCurrentMenu_();
  }

  /**
   * Refreshes the current menu, if the current action node matches the node
   * provided.
   */
  static refreshMenuForNode(node: SAChildNode): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    const actionNode = ActionManager.instance!.actionNode_;
    if (actionNode && node.equals(actionNode)) {
      ActionManager.refreshMenuUnconditionally();
    }
  }

  // ================= Private Methods ==================

  /** Returns all possible actions for the provided menu type */
  private actionsForType_(type: MenuType): MenuAction[] {
    switch (type) {
      case MenuType.MAIN_MENU:
        return [
          MenuAction.COPY,
          MenuAction.CUT,
          MenuAction.DECREMENT,
          MenuAction.DICTATION,
          MenuAction.DRILL_DOWN,
          MenuAction.INCREMENT,
          MenuAction.KEYBOARD,
          MenuAction.MOVE_CURSOR,
          MenuAction.PASTE,
          MenuAction.SCROLL_DOWN,
          MenuAction.SCROLL_LEFT,
          MenuAction.SCROLL_RIGHT,
          MenuAction.SCROLL_UP,
          MenuAction.SELECT,
          MenuAction.START_TEXT_SELECTION,
        ];

      case MenuType.TEXT_NAVIGATION:
        return [
          MenuAction.JUMP_TO_BEGINNING_OF_TEXT,
          MenuAction.JUMP_TO_END_OF_TEXT,
          MenuAction.MOVE_UP_ONE_LINE_OF_TEXT,
          MenuAction.MOVE_DOWN_ONE_LINE_OF_TEXT,
          MenuAction.MOVE_BACKWARD_ONE_WORD_OF_TEXT,
          MenuAction.MOVE_FORWARD_ONE_WORD_OF_TEXT,
          MenuAction.MOVE_BACKWARD_ONE_CHAR_OF_TEXT,
          MenuAction.MOVE_FORWARD_ONE_CHAR_OF_TEXT,
          MenuAction.END_TEXT_SELECTION,
        ];
      case MenuType.POINT_SCAN_MENU:
        return [
          MenuAction.LEFT_CLICK,
          MenuAction.RIGHT_CLICK,
        ];
      default:
        return [];
    }
  }

  private addGlobalActions_(actions: MenuAction[]): MenuAction[] {
    if (SwitchAccess.mode === Mode.POINT_SCAN) {
      actions.push(MenuAction.ITEM_SCAN);
    } else {
      actions.push(MenuAction.POINT_SCAN);
    }
    actions.push(MenuAction.SETTINGS);
    return actions;
  }

  private get currentMenuType_(): MenuType {
    return this.menuStack_[this.menuStack_.length - 1];
  }

  private getActionsForCurrentMenuAndNode_(): MenuAction[] {
    if (this.currentMenuType_ === MenuType.POINT_SCAN_MENU) {
      let actions = this.actionsForType_(MenuType.POINT_SCAN_MENU);
      actions = this.addGlobalActions_(actions);
      return actions;
    }

    if (!this.actionNode_ || !this.actionNode_.isValidAndVisible()) {
      return [];
    }
    let actions = this.actionNode_.actions as MenuAction[];
    const possibleActions = this.actionsForType_(this.currentMenuType_);
    actions = actions.filter(a => possibleActions.includes(a));
    if (this.currentMenuType_ === MenuType.MAIN_MENU) {
      actions = this.addGlobalActions_(actions);
    }
    return actions;
  }

  private getLocationForCurrentMenuAndNode_(): Rect | undefined {
    if (this.currentMenuType_ === MenuType.POINT_SCAN_MENU) {
      return {
        left: Math.floor(Navigator.byPoint.currentPoint.x),
        top: Math.floor(Navigator.byPoint.currentPoint.y),
        width: 1,
        height: 1,
      };
    }

    if (this.actionNode_) {
      return this.actionNode_.location;
    }

    return undefined;
  }

  private openCurrentMenu_(): void {
    const actions = this.getActionsForCurrentMenuAndNode_();
    const location = this.getLocationForCurrentMenuAndNode_();

    if (actions.length < 2) {
      ActionManager.exitCurrentMenu();
    }
    this.menuManager_.open(actions, location);
  }

  private performActionOnCurrentNode_(action: MenuAction): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (!this.actionNode_!.hasAction(action)) {
      ActionManager.refreshMenuUnconditionally();
      return;
    }

    // We exit the menu before asking the node to perform the action, because
    // having the menu on the group stack interferes with some actions. We do
    // not close the menu bubble until we receive the ActionResponse CLOSE_MENU.
    // If we receive a different response, we re-enter the menu.
    Navigator.byItem.suspendCurrentGroup();

    // TODO(b/314203187): Not null asserted, check that this is correct.
    const response = this.actionNode_!.performAction(action);

    switch (response) {
      case ActionResponse.CLOSE_MENU:
        ActionManager.exitAllMenus();
        return;
      case ActionResponse.EXIT_SUBMENU:
        ActionManager.exitCurrentMenu();
        return;
      case ActionResponse.REMAIN_OPEN:
        Navigator.byItem.restoreSuspendedGroup();
        return;
      case ActionResponse.RELOAD_MENU:
        ActionManager.refreshMenuUnconditionally();
        return;
      case ActionResponse.OPEN_TEXT_NAVIGATION_MENU:
        if (SwitchAccess.improvedTextInputEnabled()) {
          this.menuStack_.push(MenuType.TEXT_NAVIGATION);
        }
        this.openCurrentMenu_();
    }
  }
}

/** @type {ActionManager} */
ActionManager.instance;

TestImportManager.exportForTesting(ActionManager);
