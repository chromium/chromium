// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ArrayUtil} from '../common/array_util.js';
import {EventHandler} from '../common/event_handler.js';

import {ActionManager} from './action_manager.js';
import {Navigator} from './navigator.js';
import {SwitchAccess} from './switch_access.js';

const AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
const MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;
const ScreenRect = chrome.accessibilityPrivate.ScreenRect;
const SwitchAccessBubble = chrome.accessibilityPrivate.SwitchAccessBubble;

/**
 * Class to handle interactions with the Switch Access action menu, including
 * opening and closing the menu and setting its location / the actions to be
 * displayed.
 */
export class MenuManager {
  /** @private */
  constructor() {
    /** @private {?Array<!MenuAction>} */
    this.displayedActions_ = null;

    /** @private {ScreenRect|undefined} */
    this.displayedLocation_;

    /** @private {boolean} */
    this.isMenuOpen_ = false;

    /** @private {AutomationNode} */
    this.menuAutomationNode_;

    /** @private {!EventHandler} */
    this.clickHandler_ = new EventHandler(
        [], EventType.CLICKED, event => this.onButtonClicked_(event));
  }

  static create() {
    if (MenuManager.instance) {
      throw new Error('Cannot instantiate more than one MenuManager');
    }
    MenuManager.instance = new MenuManager();
    return MenuManager.instance;
  }

  // ================= Static Methods ==================

  /** @return {boolean} */
  static isMenuOpen() {
    return Boolean(MenuManager.instance) && MenuManager.instance.isMenuOpen_;
  }

  /** @return {AutomationNode} */
  static get menuAutomationNode() {
    if (MenuManager.instance) {
      return MenuManager.instance.menuAutomationNode_;
    }
    return null;
  }

  // ================ Instance Methods =================

  /**
   * If multiple actions are available for the currently highlighted node,
   * opens the menu. Otherwise performs the node's default action.
   * @param {!Array<!MenuAction>} actions
   * @param {ScreenRect|undefined} location
   */
  open(actions, location) {
    if (!this.isMenuOpen_) {
      if (!location) {
        return;
      }
      this.displayedLocation_ = location;
    }

    if (ArrayUtil.contentsAreEqual(actions, this.displayedActions_)) {
      return;
    }
    this.displayMenuWithActions_(actions);
  }

  /** Exits the menu. */
  close() {
    this.isMenuOpen_ = false;
    this.displayedActions_ = null;
    // To match the accessibilityPrivate function signature, displayedLocation_
    // has to be undefined rather than null.
    this.displayedLocation_ = undefined;
    Navigator.byItem.exitIfInGroup(this.menuAutomationNode_);
    this.menuAutomationNode_ = null;

    chrome.accessibilityPrivate.updateSwitchAccessBubble(
        SwitchAccessBubble.MENU, false /* show */);
  }

  // ================= Private Methods ==================

  /**
   * @param {string=} actionString
   * @return {?MenuAction}
   * @private
   */
  asAction_(actionString) {
    if (Object.values(MenuAction).includes(actionString)) {
      return /** @type {!MenuAction} */ (actionString);
    }
    return null;
  }

  /**
   * Opens or reloads the menu for the current action node with the specified
   * actions.
   * @param {!Array<!MenuAction>} actions
   * @private
   */
  displayMenuWithActions_(actions) {
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
   * @private
   */
  findAndJumpToMenu_() {
    if (this.hasMenuNode_() && this.menuAutomationNode_) {
      this.jumpToMenu_(this.menuAutomationNode_);
      return;
    }
    SwitchAccess.findNodeMatching(
        {
          role: chrome.automation.RoleType.MENU,
          attributes: {className: 'SwitchAccessMenuView'},
        },
        node => this.jumpToMenu_(node));
  }

  /** @private */
  hasMenuNode_() {
    return this.menuAutomationNode_ && this.menuAutomationNode_.role &&
        !this.menuAutomationNode_.state[chrome.automation.StateType.OFFSCREEN];
  }

  /**
   * Saves the automation node representing the menu, adds all listeners, and
   * jumps to the node.
   * @param {!AutomationNode} node
   * @private
   */
  jumpToMenu_(node) {
    if (!this.isMenuOpen_) {
      return;
    }

    // If the menu hasn't fully loaded, wait for that before jumping.
    if (node.children.length < 1 ||
        node.firstChild.state[chrome.automation.StateType.OFFSCREEN]) {
      new EventHandler(
          node, [EventType.CHILDREN_CHANGED, EventType.LOCATION_CHANGED],
          () => this.jumpToMenu_(node), {listenOnce: true})
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

/** @private */
MenuManager.instance;
