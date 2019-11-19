// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles navigation amongst the elements onscreen.
 */
class NavigationManager {
  /**
   * @param {!chrome.automation.AutomationNode} desktop
   */
  constructor(desktop) {
    /** @private {!chrome.automation.AutomationNode} */
    this.desktop_ = desktop;

    /** @private {!SARootNode} */
    this.group_ = RootNodeWrapper.buildDesktopTree(this.desktop_);

    /** @private {!SAChildNode} */
    this.node_ = this.group_.firstChild;

    /** @private {!Array<!SARootNode>} */
    this.groupStack_ = [];

    /** @private {!MenuManager} */
    this.menuManager_ = new MenuManager(this, desktop);

    /** @private {!FocusRingManager} */
    this.focusRingManager_ = new FocusRingManager();

    this.init_();
  }

  // -------------------------------------------------------
  // |                 Public Methods                      |
  // -------------------------------------------------------

  /**
   * Enters |this.node_|.
   */
  enterGroup() {
    if (!this.node_.isGroup()) {
      return;
    }

    SwitchAccessMetrics.recordMenuAction('EnterGroup');

    const newGroup = this.node_.asRootNode();
    if (newGroup) {
      this.groupStack_.push(this.group_);
      this.group_ = newGroup;
    }

    this.setNode_(this.group_.firstChild);
  }

  /**
   * Puts focus on the virtual keyboard, if the current node is a text input.
   * TODO(cbug/946190): Handle the case where the user has not enabled the
   *     onscreen keyboard.
   */
  enterKeyboard() {
    const keyboard = KeyboardRootNode.buildTree(this.desktop_);
    this.node_.performAction(SAConstants.MenuAction.OPEN_KEYBOARD);
    this.jumpTo_(keyboard);
  }

  /**
   * Open the Switch Access menu for the currently highlighted node. If there
   * are not enough actions available to trigger the menu, the current element
   * is selected.
   */
  enterMenu() {
    if (this.menuManager_.enter(this.node_)) {
      return;
    }

    // If the menu does not or cannot open, select the current node.
    this.selectCurrentNode();
  }

  /**
   * Returns the current Switch Access tree, for debugging purposes.
   * @param {boolean} wholeTree Whether to print the whole tree, or just the
   * current focus.
   * @return {!SARootNode}
   */
  getTreeForDebugging(wholeTree) {
    if (!wholeTree) {
      console.log(this.group_.debugString(wholeTree));
      return this.group_;
    }

    const desktopRoot = RootNodeWrapper.buildDesktopTree(this.desktop_);
    console.log(desktopRoot.debugString(wholeTree, '', this.node_));
    return desktopRoot;
  }

  /**
   * Move to the previous interesting node.
   */
  moveBackward() {
    if (this.menuManager_.moveBackward()) {
      // The menu navigation is handled separately. If we are in the menu, do
      // not change the primary focus node.
      return;
    }

    this.setNode_(this.node_.previous);
  }

  /**
   * Move to the next interesting node.
   */
  moveForward() {
    if (this.menuManager_.moveForward()) {
      // The menu navigation is handled separately. If we are in the menu, do
      // not change the primary focus node.
      return;
    }

    this.setNode_(this.node_.next);
  }

  /**
   * Selects the current node.
   */
  selectCurrentNode() {
    if (this.menuManager_.selectCurrentNode()) {
      // The menu navigation is handled separately. If we are in the menu, do
      // not change the primary focus node.
      return;
    }

    if (this.node_.isGroup()) {
      this.enterGroup();
      return;
    }

    if (this.node_.hasAction(SAConstants.MenuAction.OPEN_KEYBOARD)) {
      SwitchAccessMetrics.recordMenuAction(
          SAConstants.MenuAction.OPEN_KEYBOARD);
      this.node_.performAction(SAConstants.MenuAction.OPEN_KEYBOARD);
      this.enterKeyboard();
      return;
    }

    if (this.node_.hasAction(SAConstants.MenuAction.SELECT)) {
      SwitchAccessMetrics.recordMenuAction(SAConstants.MenuAction.SELECT);
      this.node_.performAction(SAConstants.MenuAction.SELECT);
    }
  }

  // -------------------------------------------------------
  // |                 Event Handlers                      |
  // -------------------------------------------------------

  /**
   * Sets up the connection between the menuPanel and menuManager.
   * @param {!PanelInterface} menuPanel
   * @return {!MenuManager}
   */
  connectMenuPanel(menuPanel) {
    menuPanel.backButtonElement().addEventListener(
        'click', this.exitGroup_.bind(this));
    this.focusRingManager_.setMenuPanel(menuPanel);
    return this.menuManager_.connectMenuPanel(menuPanel);
  }

  /**
   * When focus shifts, move to the element. Find the closest interesting
   *     element to engage with.
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onFocusChange_(event) {
    if (this.node_.isEquivalentTo(event.target)) {
      return;
    }
    this.moveTo_(event.target);
  }

  /**
   * When a menu is opened, jump focus to the menu.
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onMenuStart_(event) {
    const menuRoot = SystemMenuRootNode.buildTree(event.target);
    this.jumpTo_(menuRoot);
  }

  /**
   * Notifies focus manager that the preferences have initially loaded.
   */
  onPrefsReady() {
    this.focusRingManager_.onPrefsReady();
    this.focusRingManager_.setFocusNodes(this.node_, this.group_);
  }

  /**
   * When the automation tree changes, check if it affects any nodes we are
   *     currently listening to.
   * @param {!chrome.automation.TreeChange} treeChange
   * @private
   */
  onTreeChange_(treeChange) {
    if (treeChange.type === chrome.automation.TreeChangeType.TEXT_CHANGED) {
      return;
    }
    this.moveToValidNode_();
  }

  // -------------------------------------------------------
  // |                 Private Methods                     |
  // -------------------------------------------------------

  /**
   * Create a stack of the groups the specified node is in, and set
   *      |this.group_| to the most proximal group.
   *  @param {!chrome.automation.AutomationNode} node
   *  @private
   */
  buildGroupStack_(node) {
    // Create a list of ancestors.
    let ancestorList = [];
    while (node.parent) {
      ancestorList.push(node.parent);
      node = node.parent;
    }

    this.groupStack_ = [];
    this.group_ = RootNodeWrapper.buildDesktopTree(this.desktop_);
    while (ancestorList.length > 0) {
      let ancestor = ancestorList.pop();
      if (ancestor.role === chrome.automation.RoleType.DESKTOP) {
        continue;
      }

      if (SwitchAccessPredicate.isGroup(ancestor, this.group_)) {
        this.groupStack_.push(this.group_);
        this.group_ = RootNodeWrapper.buildTree(ancestor);
      }
    }
  }

  /**
   * Exits the current group.
   * @private
   */
  exitGroup_() {
    if (this.groupStack_.length === 0) {
      return;
    }

    this.group_.onExit();

    // Find a group that is still valid.
    do {
      this.group_ = this.groupStack_.pop();
    } while (!this.group_.isValid() && this.groupStack_.length);

    this.setNode_(this.group_.firstChild);
  }


  /** @private */
  init_() {
    if (SwitchAccess.get().prefsAreReady()) {
      this.onPrefsReady();
    }

    this.desktop_.addEventListener(
        chrome.automation.EventType.FOCUS, this.onFocusChange_.bind(this),
        false);

    this.desktop_.addEventListener(
        chrome.automation.EventType.MENU_START, this.onMenuStart_.bind(this),
        false);

    chrome.automation.addTreeChangeObserver(
        chrome.automation.TreeChangeObserverFilter.ALL_TREE_CHANGES,
        this.onTreeChange_.bind(this));
  }

  /**
   * Jumps Switch Access focus to a specified node, such as when opening a menu
   * or the keyboard. Does not modify the groups already in the group stack.
   * @param {!SARootNode} group
   * @private
   */
  jumpTo_(group) {
    this.menuManager_.exit();

    this.groupStack_.push(this.group_);
    this.group_ = group;
    this.setNode_(this.group_.firstChild);
  }

  /**
   * Moves Switch Access focus to a specified node, based on a focus shift or
   *     tree change event. Reconstructs the group stack to center on that node.
   *
   * This is a "permanent" move, while |jumpTo_| is a "temporary" move.
   *
   * @param {!chrome.automation.AutomationNode} automationNode
   * @private
   */
  moveTo_(automationNode) {
    this.buildGroupStack_(automationNode);
    let node = this.group_.firstChild;
    for (const child of this.group_.children) {
      if (child.isEquivalentTo(automationNode)) {
        node = child;
      }
    }
    if (node.equals(this.node_)) {
      return;
    }

    this.menuManager_.exit();
    this.setNode_(node);
  }

  /**
   * Moves the Switch Access focus up the group stack to the closest ancestor
   *     that hasn't been invalidated.
   * @private
   */
  moveToValidNode_() {
    if (this.node_.role && this.group_.isValid()) {
      return;
    }

    if (this.node_.role) {
      // Our group has been invalidated. Move to this node to repair the group
      // stack.
      const node = this.node_.automationNode;
      if (node) {
        this.moveTo_(node);
        return;
      }
    }

    if (this.group_.isValid()) {
      const group = this.group_.automationNode;
      if (group) {
        this.moveTo_(group);
        return;
      }
    }

    for (let group of this.groupStack_) {
      if (group.isValid()) {
        group = group.automationNode;
        if (group) {
          this.moveTo_(group);
          return;
        }
      }
    }

    // If there is no valid node in the group stack, go to the desktop.
    this.group_ = RootNodeWrapper.buildDesktopTree(this.desktop_);
    this.node_ = this.group_.firstChild;
    this.groupStack_ = [];
  }

  /**
   * Set |this.node_| to |node|, and update what is displayed onscreen.
   * @param {!SAChildNode} node
   * @private
   */
  setNode_(node) {
    this.node_.onUnfocus();
    this.node_ = node;
    this.node_.onFocus();
    this.focusRingManager_.setFocusNodes(this.node_, this.group_);
    SwitchAccess.get().restartAutoScan();
  }
}
