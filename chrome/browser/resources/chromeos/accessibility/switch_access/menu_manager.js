// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle interactions with the Switch Access menu, including moving
 * through and selecting actions.
 */
class MenuManager {
  /** @private */
  constructor() {
    /**
     * The node that was focused when the menu was opened.
     * Null if the menu is closed.
     * @private {SAChildNode}
     */
    this.actionNode_;

    /** @private {?Array<!SwitchAccessMenuAction>} */
    this.displayedActions_ = null;

    /** @private {chrome.accessibilityPrivate.ScreenRect} */
    this.displayedLocation_;

    /** @private {boolean} */
    this.isMenuOpen_ = false;

    /** @private {boolean} */
    this.inTextNavigation_ = false;

    /** @private {AutomationNode} */
    this.menuAutomationNode_;

    /** @private {!EventHandler} */
    this.clickHandler_ = new EventHandler(
        [], chrome.automation.EventType.CLICKED,
        this.onButtonClicked_.bind(this));
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
   */
  static enter() {
    const node = NavigationManager.currentNode;
    if (node.actions.length <= 1 || !node.location) {
      node.doDefaultAction();
      return;
    }

    MenuManager.instance.actionNode_ = node;
    MenuManager.instance.openMainMenu_();
  }

  /** Exits the menu. */
  static exit() {
    if (MenuManager.instance.inTextNavigation_) {
      // If we're exiting the text navigation menu, we simply return to the
      // main menu.
      MenuManager.instance.openMainMenu_();
      return;
    }

    MenuManager.instance.isMenuOpen_ = false;
    MenuManager.instance.actionNode_ = null;
    MenuManager.instance.displayedActions_ = null;
    MenuManager.instance.displayedLocation_ = null;
    NavigationManager.exitIfInGroup(MenuManager.instance.menuAutomationNode_);
    MenuManager.instance.menuAutomationNode_ = null;

    chrome.accessibilityPrivate.updateSwitchAccessBubble(
        chrome.accessibilityPrivate.SwitchAccessBubble.MENU, false /* show */);
  }

  /** @return {boolean} */
  static isMenuOpen() {
    return MenuManager.instance.isMenuOpen_;
  }

  /** @param {!SAChildNode} node */
  static reloadActionsForNode(node) {
    if (!MenuManager.isMenuOpen() ||
        !node.equals(MenuManager.instance.actionNode_)) {
      return;
    }
    MenuManager.instance.refreshActions_();
  }

  static refreshMenu() {
    if (!MenuManager.isMenuOpen()) {
      return;
    }
    MenuManager.instance.refreshActions_();
  }

  // ================= Private Methods ==================

  /**
   * @param {!Array<!SwitchAccessMenuAction>} actions
   * @return {!Array<!SwitchAccessMenuAction>}
   * @private
   */
  addGlobalActions_(actions) {
    actions.push(SwitchAccessMenuAction.SETTINGS);
    return actions;
  }

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
    const location = this.displayedLocation_ || this.actionNode_.location;
    chrome.accessibilityPrivate.updateSwitchAccessBubble(
        chrome.accessibilityPrivate.SwitchAccessBubble.MENU, true /* show */,
        location, actions);

    this.isMenuOpen_ = true;
    this.findAndJumpToMenuAutomationNode_();
    this.displayedActions_ = actions;
    this.displayedLocation_ = location;
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
    }
    SwitchAccess.findNodeMatching(
        {
          role: chrome.automation.RoleType.MENU,
          attributes: {className: 'SwitchAccessMenuView'}
        },
        this.jumpToMenuAutomationNode_.bind(this));
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
      default:
        return false;
    }
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
            chrome.automation.EventType.LOCATION_CHANGED
          ],
          this.jumpToMenuAutomationNode_.bind(this, node), {listenOnce: true})
          .start();
      return;
    }

    this.menuAutomationNode_ = node;
    this.clickHandler_.setNodes(this.menuAutomationNode_);
    this.clickHandler_.start();
    NavigationManager.jumpToSwitchAccessMenu(this.menuAutomationNode_);
  }

  /**
   * Listener for when buttons are clicked. Identifies the action to perform
   * and forwards the request to the current node.
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onButtonClicked_(event) {
    const selectedAction = this.asAction_(event.target.value);
    if (!this.isMenuOpen_ || !selectedAction ||
        this.handleGlobalActions_(selectedAction)) {
      return;
    }

    if (!this.actionNode_.hasAction(selectedAction)) {
      this.refreshActions_();
      return;
    }

    // We exit the menu before asking the node to perform the action, because
    // having the menu on the group stack interferes with some actions. We do
    // not close the menu bubble until we receive the ActionResponse CLOSE_MENU.
    // If we receive a different response, we re-enter the menu.
    NavigationManager.exitIfInGroup(this.menuAutomationNode_);
    const response = this.actionNode_.performAction(selectedAction);
    if (response === SAConstants.ActionResponse.CLOSE_MENU ||
        !this.hasValidMenuAutomationNode_()) {
      MenuManager.exit();
    } else {
      NavigationManager.jumpToSwitchAccessMenu(this.menuAutomationNode_);
    }

    switch (response) {
      case SAConstants.ActionResponse.RELOAD_MAIN_MENU:
        this.refreshActions_();
        break;
      case SAConstants.ActionResponse.OPEN_TEXT_NAVIGATION_MENU:
        this.openTextNavigation_();
    }
  }

  /** @private */
  openMainMenu_() {
    this.inTextNavigation_ = false;
    let actions = this.actionNode_.actions;
    actions = this.addGlobalActions_(actions);
    actions = actions.filter((a) => !this.textNavigationActions_.includes(a));

    if (ArrayUtil.contentsAreEqual(actions, this.displayedActions_)) {
      return;
    }
    this.displayMenuWithActions_(actions);
  }

  /** @private */
  openTextNavigation_() {
    if (!SwitchAccess.instance.improvedTextInputEnabled()) {
      this.openMainMenu_();
      return;
    }

    this.inTextNavigation_ = true;
    this.displayMenuWithActions_(this.textNavigationActions_);
  }

  /**
   * Checks if we can still show a menu for the node, and if so, changes the
   * actions displayed in the menu.
   * @private
   */
  refreshActions_() {
    if (!this.actionNode_.isValidAndVisible() ||
        this.actionNode_.actions.length <= 1) {
      MenuManager.exit();
      return;
    }

    this.openMainMenu_();
  }

  /**
   * @return {!Array<!SwitchAccessMenuAction>}
   * @private
   */
  get textNavigationActions_() {
    const actions = [
      SwitchAccessMenuAction.JUMP_TO_BEGINNING_OF_TEXT,
      SwitchAccessMenuAction.JUMP_TO_END_OF_TEXT,
      SwitchAccessMenuAction.MOVE_UP_ONE_LINE_OF_TEXT,
      SwitchAccessMenuAction.MOVE_DOWN_ONE_LINE_OF_TEXT,
      SwitchAccessMenuAction.MOVE_BACKWARD_ONE_WORD_OF_TEXT,
      SwitchAccessMenuAction.MOVE_FORWARD_ONE_WORD_OF_TEXT,
      SwitchAccessMenuAction.MOVE_BACKWARD_ONE_CHAR_OF_TEXT,
      SwitchAccessMenuAction.MOVE_FORWARD_ONE_CHAR_OF_TEXT,
    ];
    if (SwitchAccess.instance.improvedTextInputEnabled() &&
        TextNavigationManager.currentlySelecting()) {
      actions.unshift(SwitchAccessMenuAction.END_TEXT_SELECTION);
    }
    return actions;
  }
}
