// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ArrayUtil} from '/common/array_util.js';
import {EventHandler} from '/common/event_handler.js';

import {ActionManager} from './action_manager.js';
import {Navigator} from './navigator.js';
import {SwitchAccess} from './switch_access.js';

import AutomationEvent = chrome.automation.AutomationEvent;
import AutomationNode = chrome.automation.AutomationNode;
import EventType = chrome.automation.EventType;
import MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;
import RoleType = chrome.automation.RoleType;
import ScreenRect = chrome.accessibilityPrivate.ScreenRect;
import StateType = chrome.automation.StateType;
import SwitchAccessBubble = chrome.accessibilityPrivate.SwitchAccessBubble;

interface EventHandlerOptions {
  capture: boolean | undefined;
  exactMatch: boolean | undefined;
  listenOnce: boolean | undefined;
  predicate: ((arg: any) => boolean) | undefined;
}

/**
 * Class to handle interactions with the Switch Access action menu, including
 * opening and closing the menu and setting its location / the actions to be
 * displayed.
 */
export class MenuManager {
  private displayedActions_: MenuAction[] | null = null;
  private displayedLocation_?: ScreenRect;
  private isMenuOpen_ = false;
  private menuAutomationNode_?: AutomationNode | null;
  private clickHandler_: EventHandler;

  static instance?: MenuManager;

  private constructor() {
    this.clickHandler_ = new EventHandler(
        [], EventType.CLICKED,
        (event: AutomationEvent) => this.onButtonClicked_(event));
  }

  static create(): MenuManager {
    if (MenuManager.instance) {
      throw new Error('Cannot instantiate more than one MenuManager');
    }
    MenuManager.instance = new MenuManager();
    return MenuManager.instance;
  }

  // ================= Static Methods ==================

  static isMenuOpen(): boolean {
    // TODO(b/314203187): Not nulls asserted, check that this is correct.
    return Boolean(MenuManager.instance) && MenuManager.instance!.isMenuOpen_;
  }

  static get menuAutomationNode(): AutomationNode | null | undefined {
    if (MenuManager.instance) {
      return MenuManager.instance.menuAutomationNode_;
    }
    return null;
  }

  // ================ Instance Methods =================

  /**
   * If multiple actions are available for the currently highlighted node,
   * opens the menu. Otherwise performs the node's default action.
   */
  open(actions: MenuAction[], location?: ScreenRect): void {
    if (!this.isMenuOpen_) {
      if (!location) {
        return;
      }
      this.displayedLocation_ = location;
    }

    if (ArrayUtil.contentsAreEqual(
        actions, this.displayedActions_ ?? undefined)) {
      return;
    }
    this.displayMenuWithActions_(actions);
  }

  /** Exits the menu. */
  close(): void {
    this.isMenuOpen_ = false;
    this.displayedActions_ = null;
    // To match the accessibilityPrivate function signature, displayedLocation_
    // has to be undefined rather than null.
    this.displayedLocation_ = undefined;
    Navigator.byItem.exitIfInGroup(this.menuAutomationNode_ ?? null);
    this.menuAutomationNode_ = null;

    chrome.accessibilityPrivate.updateSwitchAccessBubble(
        SwitchAccessBubble.MENU, false /* show */);
  }

  // ================= Private Methods ==================

  private asAction_(actionString: string | undefined): MenuAction | null {
    if (Object.values(MenuAction).includes(actionString as MenuAction)) {
      return actionString as MenuAction;
    }
    return null;
  }

  /**
   * Opens or reloads the menu for the current action node with the specified
   * actions.
   */
  private displayMenuWithActions_(actions: MenuAction[]): void {
    chrome.accessibilityPrivate.updateSwitchAccessBubble(
        SwitchAccessBubble.MENU, true /* show */, this.displayedLocation_,
        actions);

    this.isMenuOpen_ = true;
    this.findAndJumpToMenu_();
    this.displayedActions_ = actions;
  }

  /**
   * Searches the automation tree to find the node for the Switch Access menu.
   * If we've already found a node, and it's still valid, then jump to that
   * node.
   */
  private findAndJumpToMenu_(): void {
    if (this.hasMenuNode_() && this.menuAutomationNode_) {
      this.jumpToMenu_(this.menuAutomationNode_);
      return;
    }
    SwitchAccess.findNodeMatching(
        {
          role: RoleType.MENU,
          attributes: {className: 'SwitchAccessMenuView'},
        },
        node => this.jumpToMenu_(node));
  }

  private hasMenuNode_(): boolean {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    return Boolean(this.menuAutomationNode_ &&
        this.menuAutomationNode_.role &&
        !this.menuAutomationNode_.state![StateType.OFFSCREEN]);
  }

  /**
   * Saves the automation node representing the menu, adds all listeners, and
   * jumps to the node.
   */
  private jumpToMenu_(node: AutomationNode): void {
    if (!this.isMenuOpen_) {
      return;
    }

    // If the menu hasn't fully loaded, wait for that before jumping.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (node.children.length < 1 ||
        node.firstChild!.state![StateType.OFFSCREEN]) {
      new EventHandler(
          node, [EventType.CHILDREN_CHANGED, EventType.LOCATION_CHANGED],
          () => this.jumpToMenu_(node),
          {listenOnce: true} as EventHandlerOptions)
          .start();
      return;
    }

    this.menuAutomationNode_ = node;
    this.clickHandler_.setNodes(this.menuAutomationNode_);
    this.clickHandler_.start();
    Navigator.byItem.jumpTo(this.menuAutomationNode_);
  }

  /**
   * Listener for when buttons are clicked. Identifies the action to perform
   * and forwards the request to the action manager.
   */
  private onButtonClicked_(event: AutomationEvent): void {
    const selectedAction = this.asAction_(event.target.value);
    if (!this.isMenuOpen_ || !selectedAction) {
      return;
    }
    ActionManager.performAction(selectedAction as MenuAction);
  }
}
