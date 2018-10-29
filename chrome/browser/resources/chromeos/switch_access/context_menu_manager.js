// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle interactions with the context menu, including moving through
 * and selecting actions.
 */

class ContextMenuManager {
  /**
   * @param {!NavigationManager} navigationManager
   * @param {!chrome.automation.AutomationNode} desktop
   */
  constructor(navigationManager, desktop) {
    /**
     * A list of the ContextMenu actions that are currently enabled.
     * @private {!Array<ContextMenuManager.Action>}
     */
    this.actions_ = [];

    /**
     * The parent automation manager.
     * @private {!NavigationManager}
     */
    this.navigationManager_ = navigationManager;

    /**
     * The root node of the screen.
     * @private {!chrome.automation.AutomationNode}
     */
    this.desktop_ = desktop;

    /**
     * The root node of the context menu.
     * @private {chrome.automation.AutomationNode}
     */
    this.menuNode_;

    /**
     * The current node of the context menu.
     * @private {chrome.automation.AutomationNode}
     */
    this.node_;

    /**
     * Keeps track of when we're in the context menu.
     * @private {boolean}
     */
    this.inContextMenu_ = false;

    this.init_();
  }

  /**
   * Enter the context menu and highlight the first available action.
   *
   * @param {!Array<!ContextMenuManager.Action>} actions
   */
  enter(actions) {
    this.inContextMenu_ = true;
    if (actions !== this.actions_) {
      this.actions_ = actions;
      MessageHandler.sendMessage(
          MessageHandler.Destination.PANEL, 'setActions', actions);
    }

    this.node_ = this.menuNode();
    this.moveForward();
  }

  /**
   * Exits the context menu.
   */
  exit() {
    this.clearFocusRing_();
    this.inContextMenu_ = false;
    if (this.node_)
      this.node_ = null;
  }

  /**
   * Move to the next available action in the menu. If this is no next action,
   * select the whole context menu to loop again.
   * @return {boolean} Whether this function had any effect.
   */
  moveForward() {
    if (!this.node_ || !this.inContextMenu_)
      return false;

    this.clearFocusRing_();
    const treeWalker = new AutomationTreeWalker(
        this.node_, constants.Dir.FORWARD,
        SwitchAccessPredicate.restrictions(this.menuNode()));
    const node = treeWalker.next().node;
    if (!node)
      this.node_ = this.menuNode();
    else
      this.node_ = node;
    this.updateFocusRing_();
    return true;
  }

  /**
   * Move to the previous available action in the context menu. If we're at the
   * beginning of the list, start again at the end.
   * @return {boolean} Whether this function had any effect.
   */
  moveBackward() {
    if (!this.node_ || !this.inContextMenu_)
      return false;

    this.clearFocusRing_();
    const treeWalker = new AutomationTreeWalker(
        this.node_, constants.Dir.BACKWARD,
        SwitchAccessPredicate.restrictions(this.menuNode()));
    let node = treeWalker.next().node;

    // If node is null, find the last enabled button.
    let lastChild = this.menuNode().lastChild;
    while (!node && lastChild) {
      if (SwitchAccessPredicate.isActionable(lastChild)) {
        node = lastChild;
        break;
      } else {
        lastChild = lastChild.previousSibling;
      }
    }

    this.node_ = node;
    this.updateFocusRing_();
    return true;
  }

  /**
   * Perform the action indicated by the current button (or no action if the
   * entire menu is selected). Then exit the context menu and return to
   * traditional navigation.
   * @return {boolean} Whether this function had any effect.
   */
  selectCurrentNode() {
    if (!this.node_ || !this.inContextMenu_)
      return false;

    this.clearFocusRing_();
    this.node_.doDefault();
    this.exit();
    return true;
  }

  /**
   * Get the menu node. If it's not defined, search for it.
   * @return {!chrome.automation.AutomationNode}
   */
  menuNode() {
    if (this.menuNode_)
      return this.menuNode_;

    const treeWalker = new AutomationTreeWalker(
        this.desktop_, constants.Dir.FORWARD,
        SwitchAccessPredicate.contextMenuDiscoveryRestrictions());
    const node = treeWalker.next().node;
    if (node) {
      this.menuNode_ = node;
      return this.menuNode_;
    }
    console.log('Unable to find the context menu.');
    return this.desktop_;
  }

  /**
   * Clear the focus ring.
   * @private
   */
  clearFocusRing_() {
    this.updateFocusRing_(true);
  }

  /**
   * @private
   */
  init_() {
    // Listen for messages from the menu.
    window.addEventListener('message', this.onMessageReceived_.bind(this));
  }

  /**
   * Receive a message from the context menu, and perform the appropriate
   * action.
   * @private
   */
  onMessageReceived_(event) {
    this.exit();

    if (event.data === ContextMenuManager.Action.CLICK)
      this.navigationManager_.selectCurrentNode();
    else if (event.data === ContextMenuManager.Action.DICTATION)
      chrome.accessibilityPrivate.toggleDictation();
    else if (event.data === ContextMenuManager.Action.OPTIONS)
      window.switchAccess.showOptionsPage();
    else if (
        event.data === ContextMenuManager.Action.SCROLL_DOWN ||
        event.data === ContextMenuManager.Action.SCROLL_UP ||
        event.data === ContextMenuManager.Action.SCROLL_LEFT ||
        event.data === ContextMenuManager.Action.SCROLL_RIGHT)
      this.navigationManager_.scroll(event.data);
  }

  /**
   * Send a message to the context menu to update the focus ring around the
   * current node.
   * TODO(zhelfins): Revisit focus rings before launch
   * @private
   * @param {boolean=} opt_clear If true, will clear the focus ring.
   */
  updateFocusRing_(opt_clear) {
    if (!this.node_)
      return;
    const id = this.node_.htmlAttributes.id;
    const onOrOff = opt_clear ? 'off' : 'on';
    MessageHandler.sendMessage(
        MessageHandler.Destination.PANEL, 'setFocusRing', [id, onOrOff]);
  }
}

/**
 * Actions available in the Context Menu.
 * @enum {string}
 * @const
 */
ContextMenuManager.Action = {
  CLICK: 'click',
  DICTATION: 'dictation',
  OPTIONS: 'options',
  SCROLL_BACKWARD: 'scroll-backward',
  SCROLL_DOWN: 'scroll-down',
  SCROLL_FORWARD: 'scroll-forward',
  SCROLL_LEFT: 'scroll-left',
  SCROLL_RIGHT: 'scroll-right',
  SCROLL_UP: 'scroll-up'
};

/**
 * The ID for the div containing the context menu.
 * @const
 */
ContextMenuManager.MenuId = 'switchaccess_contextmenu_actions';
