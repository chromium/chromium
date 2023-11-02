// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ArrayUtil} from '../common/array_util.js';
import {EventHandler} from '../common/event_handler.js';

import {ActionManager} from './action_manager.js';
import {Navigator} from './navigator.js';
import {SwitchAccess} from './switch_access.js';
import {SwitchAccessMenuAction} from './switch_access_constants.js';

const AutomationNode = chrome.automation.AutomationNode;

/**
 * Class to handle interactions with the Switch Access action menu, including
 * opening and closing the menu and setting its location / the actions to be
 * displayed.
 */
export class MenuManager {
  /** @private */
  constructor() {
    /** @private {?Array<!SwitchAccessMenuAction>} */
    this.displayedActions_ = null;

    /** @private {chrome.accessibilityPrivate.ScreenRect} */
    this.displayedLocation_;

    /** @private {boolean} */
    this.isMenuOpen_ = false;

    /** @private {AutomationNode} */
    this.menuAutomationNode_;

    /** @private {!EventHandler} */
    this.clickHandler_ = new EventHandler(
        [], chrome.automation.EventType.CLICKED,
        event => this.onButtonClicked_(event));
  }

  static get instance() {
    if (!MenuManager.instance_) {
      MenuManager.instance_ = new MenuManager();
    }
    return MenuManager.instance_;
  }

  // ================= Static Methods ==================

  /**
   * If multiple actions are available for the currently highlighted node,
   * opens the menu. Otherwise performs the node's default action.
   * @param {!Array<!SwitchAccessMenuAction>} actions
   * @param {chrome.accessibilityPrivate.ScreenRect|undefined} location
   */
  static open(actions, location) {
    if (!MenuManager.instance.isMenuOpen_) {
      if (!location) {
        return;
      }
      MenuManager.instance.displayedLocation_ = location;
    }

    if (ArrayUtil.contentsAreEqual(
            actions, MenuManager.instance.displayedActions_)) {
      return;
    }
    MenuManager.instance.displayMenuWithActions_(actions);
  }

  /** Exits the menu. */
  static close() {
    MenuManager.instance.isMenuOpen_ = false;
    MenuManager.instance.actionNode_ = null;
    MenuManager.instance.displayedActions_ = null;
    MenuManager.instance.displayedLocation_ = null;
    Navigator.byItem.exitIfInGroup(MenuManager.instance.menuAutomationNode_);
    MenuManager.instance.menuAutomationNode_ = null;

    chrome.accessibilityPrivate.updateSwitchAccessBubble(
        chrome.accessibilityPrivate.SwitchAccessBubble.MENU, false /* show */);
  }

  /** @return {boolean} */
  static isMenuOpen() {
    return MenuManager.instance.isMenuOpen_;
  }

  /** @return {!AutomationNode} */
  static get menuAutomationNode() {
    return MenuManager.instance.menuAutomationNode_;
  }

  // ================= Private Methods ==================

  /**
   * @param {string=} actionString
   * @return {?SwitchAccessMenuAction}
   * @private
   */
  asAction_(actionString) {
    if (Object.values(SwitchAccessMenuAction).includes(actionString)) {
      return /** @type {!SwitchAccessMenuAction} */ (actionString);
    }
    return null;
  }

  /**
   * Opens or reloads the menu for the current action node with the specified
   * actions.
   * @param {!Array<SwitchAccessMenuAction>} actions
   * @private
   */
  displayMenuWithActions_(actions) {
    chrome.accessibilityPrivate.updateSwitchAccessBubble(
        chrome.accessibilityPrivate.SwitchAccessBubble.MENU, true /* show */,
        this.displayedLocation_, actions);

    this.isMenuOpen_ = true;
    this.findAndJumpToMenuAutomationNode_();
    this.displayedActions_ = actions;
  }

  /**
   * Searches the automation tree to find the node for the Switch Access menu.
   * If we've already found a node, and it's still valid, then jump to that
   * node.
   * @private
   */
  findAndJumpToMenuAutomationNode_() {
    if (this.hasValidMenuAutomationNode_() && this.menuAutomationNode_) {
      this.jumpToMenuAutomationNode_(this.menuAutomationNode_);
      return;
    }
    SwitchAccess.findNodeMatching(
        {
          role: chrome.automation.RoleType.MENU,
          attributes: {className: 'SwitchAccessMenuView'},
        },
        node => this.jumpToMenuAutomationNode_(node));
  }

  /** @private */
  hasValidMenuAutomationNode_() {
    return this.menuAutomationNode_ && this.menuAutomationNode_.role &&
        !this.menuAutomationNode_.state[chrome.automation.StateType.OFFSCREEN];
  }

  /**
   * Saves the automation node representing the menu, adds all listeners, and
   * jumps to the node.
   * @param {!AutomationNode} node
   * @private
   */
  jumpToMenuAutomationNode_(node) {
    if (!this.isMenuOpen_) {
      return;
    }

    // If the menu hasn't fully loaded, wait for that before jumping.
    if (node.children.length < 1 ||
        node.firstChild.state[chrome.automation.StateType.OFFSCREEN]) {
      new EventHandler(
          node,
          [
            chrome.automation.EventType.CHILDREN_CHANGED,
            chrome.automation.EventType.LOCATION_CHANGED,
          ],
          () => this.jumpToMenuAutomationNode_(node), {listenOnce: true})
          .start();
      return;
    }

    this.menuAutomationNode_ = node;
    this.clickHandler_.setNodes(this.menuAutomationNode_);
    this.clickHandler_.start();
    Navigator.byItem.jumpToSwitchAccessMenu();
  }

  /**
   * Listener for when buttons are clicked. Identifies the action to perform
   * and forwards the request to the action manager.
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onButtonClicked_(event) {
    const selectedAction = this.asAction_(event.target.value);
    if (!this.isMenuOpen_ || !selectedAction) {
      return;
    }
    ActionManager.performAction(selectedAction);
  }
}
