// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle interactions with the Switch Access menu, including moving
 * through and selecting actions.
 */

class MenuManager {
  /**
   * @param {!NavigationManager} navigationManager
   * @param {!chrome.automation.AutomationNode} desktop
   */
  constructor(navigationManager, desktop) {
    /**
     * A list of the Menu actions that are currently enabled.
     * @private {!Array<!SAConstants.MenuAction>}
     */
    this.actions_ = [];

    /**
     * The parent automation manager.
     * @private {!NavigationManager}
     */
    this.navigationManager_ = navigationManager;

    /**
     * The text navigation manager.
     * @private {!TextNavigationManager}
     */
    this.textNavigationManager_ = new TextNavigationManager();

    /**
     * The root node of the screen.
     * @private {!chrome.automation.AutomationNode}
     */
    this.desktop_ = desktop;

    /**
     * The root node of the menu panel.
     * @private {chrome.automation.AutomationNode}
     */
    this.menuPanelNode_;

    /**
     * The root node of the menu.
     * @private {SARootNode}
     */
    this.menuNode_;

    /**
     * The current node of the menu.
     * @private {SAChildNode}
     */
    this.node_;

    /**
     * The node that the menu has been opened for. Null if the menu is not
     * currently open.
     * @private {SAChildNode}
     */
    this.menuOriginNode_;

    /**
     * Keeps track of when we're in the Switch Access menu.
     * @private {boolean}
     */
    this.inMenu_ = false;

    /**
     * Keeps track of when there's a selection in the current node.
     * @private {boolean}
     */
    this.selectionExists_ = false;

    /**
     * A function to be called when the menu exits.
     * @private {?function()}
     */
    this.onExitCallback_ = null;

    /**
     * Keeps track of when the clipboard is empty.
     * @private {boolean}
     */
    this.clipboardHasData_ = false;

    /**
     * A reference to the Switch Access Menu Panel class.
     * @private {PanelInterface}
     */
    this.menuPanel_;

    /**
     * Callback for highlighting the first available action once a menu has been
     *     loaded in the panel. This function is created here, rather than below
     *     with the other methods, because we need a consistent function object
     *     to be able to add and remove the listener. If this function were a
     *     method, it would need to have the |this| reference bound to it, and
     *     each call to |.bind()| creates a new function (meaning the listener
     *     could never be removed).
     * Why does this work? While methods use call scoping (they look for
     *     variables in the context of the call site), fat arrow functions,
     *     like below, use lexical scoping (they look for variables in the
     *     context of where the function was declared). So the proper |this|
     *     object is referenced without the need for binding.
     * @private {function()}
     */
    this.onMenuPanelChildrenChanged_ = () => {
      this.buildMenuTree_();
      this.highlightFirstAction_();
    };

    /**
     * A stack to keep track of all menus that have been opened before
     * the current menu (so the top of the stack will be the parent
     * menu of the current menu).
     * @private {!Array<SAConstants.MenuId>}
     */
    this.menuStack_ = [];

    this.init_();
  }

  /**
   * Set up clipboardListener for showing/hiding paste button.
   * @private
   */
  init_() {
    if (SwitchAccess.get().improvedTextInputEnabled()) {
      chrome.clipboard.onClipboardDataChanged.addListener(
          this.updateClipboardHasData.bind(this));
    }
  }

  /**
   * If multiple actions are available for the currently highlighted node,
   * opens the main menu. Otherwise, selects the node by default.
   * @param {!SAChildNode} navNode the currently highlighted node, for which the
   *     menu is to be displayed.
   * @return {boolean} True if the menu opened or an action was selected, false
   *     otherwise.
   */
  enter(navNode) {
    if (!this.menuPanel_) {
      console.log('Error: Menu panel has not loaded.');
      return false;
    }

    // If the menu is already open, select the highlighted element.
    if (this.selectCurrentNode()) {
      return true;
    }

    if (!this.openMenu_(navNode, SAConstants.MenuId.MAIN)) {
      // openMenu_ will return false (indicating that the menu was not opened
      // successfully) when there is only one interesting action (selection)
      // specific to this node. In this case, rather than forcing the user to
      // repeatedly disambiguate, we will simply select by default.
      return false;
    }

    this.inMenu_ = true;
    return true;
  }

  /**
   * Exits the menu.
   */
  exit() {
    if (!this.inMenu_) {
      return;
    }

    this.closeCurrentMenu_();
    this.inMenu_ = false;

    if (this.onExitCallback_) {
      this.onExitCallback_();
      this.onExitCallback_ = null;
    }
    this.menuOriginNode_ = null;

    chrome.accessibilityPrivate.setSwitchAccessMenuState(
        false /** should_show */, RectHelper.ZERO_RECT, 0);
  }

  /**
   * Opens the menu with given |menuId|. Shows the menu actions that are
   * applicable to the currently highlighted node in the menu panel. If the
   * menu being opened is the same as the current menu open (i.e. the menu is
   * being reloaded), then the action that triggered the reload
   * will be highlighted. Otherwise, the first available action will
   * be highlighted. Returns a boolean of whether or not the menu was
   * successfully opened.
   * @param {!SAChildNode} navNode The currently highlighted node, for which the
   *     menu is being opened.
   * @param {!SAConstants.MenuId} menuId Indicates the menu being opened.
   * @param {boolean=} isSubmenu Whether or not the menu being opened is a
   *     submenu of the current menu.
   * @return {boolean} Whether or not the menu was successfully opened.
   * @private
   */
  openMenu_(navNode, menuId, isSubmenu = false) {
    // Action currently highlighted in the menu (null if the menu was closed
    // before this function was called).
    let actionNode = null;
    if (this.node_) {
      actionNode = this.node_.automationNode;
    }

    const currentMenuId = this.menuPanel_.currentMenuId();
    const shouldReloadMenu = (currentMenuId === menuId);

    if (!shouldReloadMenu) {
      // Close the current menu before opening a new one.
      this.closeCurrentMenu_();

      if (currentMenuId && isSubmenu) {
        // Opening a submenu, so push the parent menu onto the stack.
        this.menuStack_.push(currentMenuId);
      }
    }

    const actions = this.getMenuActions_(navNode, menuId);

    if (!actions) {
      return false;
    }

    // Converting to JSON strings to check equality of Array contents.
    if (JSON.stringify(actions) !== JSON.stringify(this.actions_)) {
      // Set new menu actions in the panel.
      this.actions_ = actions;
      this.menuPanel_.setActions(this.actions_, menuId);
    }

    const loc = navNode.location;
    if (!loc) {
      console.log('Unable to show Switch Access menu.');
      return false;
    }
    // Show the menu panel.
    chrome.accessibilityPrivate.setSwitchAccessMenuState(
        true, loc, actions.length);

    this.menuOriginNode_ = navNode;

    const autoNode = this.menuOriginNode_.automationNode;
    if (autoNode && !shouldReloadMenu &&
        SwitchAccess.get().improvedTextInputEnabled()) {
      const callback = this.reloadMenuForSelectionChange_.bind(this);

      autoNode.addEventListener(
          chrome.automation.EventType.TEXT_SELECTION_CHANGED, callback,
          false /** use_capture */);
      this.onExitCallback_ = autoNode.removeEventListener.bind(
          autoNode, chrome.automation.EventType.TEXT_SELECTION_CHANGED,
          callback, false /** use_capture */);
    }

    if (shouldReloadMenu) {
      this.buildMenuTree_();
      let buttonId = actionNode ? actionNode.htmlAttributes.id : '';
      if (actions.includes(buttonId)) {
        // Highlight the same action that was highlighted before the menu was
        // reloaded.
        this.updateFocusRing_();
      } else {
        this.highlightFirstAction_();
      }
    } else {
      // Wait for the menu to appear in the panel before highlighting the
      // first available action.
      this.menuPanelNode_.addEventListener(
          chrome.automation.EventType.CHILDREN_CHANGED,
          this.onMenuPanelChildrenChanged_, false /** use_capture */);
    }

    return true;
  }

  /**
   * Closes the current menu and clears the menu panel.
   * @private
   */
  closeCurrentMenu_() {
    this.clearFocusRing_();
    if (this.node_) {
      this.node_ = null;
    }
    this.menuPanel_.clear();
    this.actions_ = [];
    this.menuNode_ = null;
  }

  /**
   * Get the actions applicable for |navNode| from the menu with given
   * |menuId|.
   * @param {!SAChildNode} navNode The currently selected node, for which the
   *     menu is being opened.
   * @param {SAConstants.MenuId} menuId
   * @return {Array<SAConstants.MenuAction>}
   * @private
   */
  getMenuActions_(navNode, menuId) {
    switch (menuId) {
      case SAConstants.MenuId.MAIN:
        return this.getMainMenuActionsForNode_(navNode);
      case SAConstants.MenuId.TEXT_NAVIGATION:
        return this.getTextNavigationActions_();
      default:
        return this.getMainMenuActionsForNode_(navNode);
    }
  }

  /**
   * Get the actions in the text navigation submenu.
   * @return {!Array<SAConstants.MenuAction>}
   * @private
   */
  getTextNavigationActions_() {
    return [
      SAConstants.MenuAction.JUMP_TO_BEGINNING_OF_TEXT,
      SAConstants.MenuAction.JUMP_TO_END_OF_TEXT,
      SAConstants.MenuAction.MOVE_BACKWARD_ONE_CHAR_OF_TEXT,
      SAConstants.MenuAction.MOVE_BACKWARD_ONE_WORD_OF_TEXT,
      SAConstants.MenuAction.MOVE_DOWN_ONE_LINE_OF_TEXT,
      SAConstants.MenuAction.MOVE_FORWARD_ONE_CHAR_OF_TEXT,
      SAConstants.MenuAction.MOVE_FORWARD_ONE_WORD_OF_TEXT,
      SAConstants.MenuAction.MOVE_UP_ONE_LINE_OF_TEXT
    ];
  }

  /**
   * Highlights the first available action in the menu.
   * @private
   */
  highlightFirstAction_() {
    if (!this.menuNode_) {
      return;
    }
    this.node_ = this.menuNode_.firstChild;
    this.updateFocusRing_();

    // The event is fired multiple times when a new menu is opened in the
    // panel, so remove the listener once the callback has been called once.
    // This ensures the first action is not continually highlighted as we
    // navigate through the menu.
    this.menuPanelNode_.removeEventListener(
        chrome.automation.EventType.CHILDREN_CHANGED,
        this.onMenuPanelChildrenChanged_, false /** Don't use capture. */);
  }

  /**
   * Move to the next available action in the menu. If this is no next action,
   * focus the whole menu to loop again.
   * @return {boolean} Whether this function had any effect.
   */
  moveForward() {
    if (!this.inMenu_ || !this.node_) {
      return false;
    }

    this.clearFocusRing_();
    this.node_ = this.node_.next;
    this.updateFocusRing_();
    return true;
  }

  /**
   * Move to the previous available action in the menu. If we're at the
   * beginning of the list, start again at the end.
   * @return {boolean} Whether this function had any effect.
   */
  moveBackward() {
    if (!this.inMenu_ || !this.node_) {
      return false;
    }

    this.clearFocusRing_();
    this.node_ = this.node_.previous;
    this.updateFocusRing_();
    return true;
  }

  /**
   * Perform the action indicated by the current button.
   * @return {boolean} Whether this function had any effect.
   */
  selectCurrentNode() {
    if (!this.inMenu_ || !this.node_) {
      return false;
    }

    if (this.node_ instanceof BackButtonNode) {
      // The back button was selected.
      this.selectBackButton();
    } else {
      // A menu action was selected.
      this.node_.performAction(SAConstants.MenuAction.SELECT);
    }
    return true;
  }

  /**
   * Selects the back button for the menu. If the current menu is a submenu
   * (i.e. not the main menu), then the current menu will be
   * closed and the parent menu that opened the current menu will be re-opened.
   * If the current menu is the main menu, then exit the menu panel entirely
   * and return to traditional navigation.
   */
  selectBackButton() {
    // Id of the menu that opened the current menu (null if the current
    // menu is the main menu and not a submenu).
    const parentMenuId = this.menuStack_.pop();
    if (parentMenuId && this.menuOriginNode_) {
      // Re-open the parent menu.
      this.openMenu_(this.menuOriginNode_, parentMenuId);
    } else {
      this.exit();
    }
  }

  /**
   * Sets up the connection between the menuPanel and the menuManager.
   * @param {!PanelInterface} menuPanel
   * @return {!MenuManager}
   */
  connectMenuPanel(menuPanel) {
    this.menuPanel_ = menuPanel;
    this.findMenuPanelNode_();
    return this;
  }

  /**
   * Searches for the menu panel node.
   */
  findMenuPanelNode_() {
    const treeWalker = new AutomationTreeWalker(
        this.desktop_, constants.Dir.FORWARD,
        SwitchAccessPredicate.switchAccessMenuPanelDiscoveryRestrictions());
    const node = treeWalker.next().node;
    if (!node) {
      setTimeout(this.findMenuPanelNode_.bind(this), 500);
      return;
    }
    this.menuPanelNode_ = node;
    this.buildMenuTree_();
  }

  /**
   * Builds the tree for the current menu.
   */
  buildMenuTree_() {
    // menu_panel.html controls the contents of the menu panel, and we are
    // guaranteed that the menu will be the first child.
    if (this.menuPanelNode_ && this.menuPanelNode_.firstChild) {
      this.menuNode_ =
          RootNodeWrapper.buildTree(this.menuPanelNode_.firstChild);
    }
  }

  /**
   * TODO(rosalindag): Add functionality to catch when clipboardHasData_ needs
   * to be set to false.
   * Set the clipboardHasData variable to true and reload the menu.
   */
  updateClipboardHasData() {
    this.clipboardHasData_ = true;
    if (this.menuOriginNode_) {
      this.openMenu_(this.menuOriginNode_, SAConstants.MenuId.MAIN);
    }
  }

  /**
   * Clear the focus ring.
   * @private
   */
  clearFocusRing_() {
    this.updateFocusRing_(true);
  }

  /**
   * Returns if there is a selection in the current node.
   * @private
   * @returns {boolean} whether or not there's a selection
   */
  nodeHasSelection_() {
    const node = this.menuOriginNode_.automationNode;

    if (node && node.textSelStart !== node.textSelEnd) {
      return true;
    }
    return false;
  }

  /**
   * Check to see if there is a change in the selection in the current node and
   * reload the menu if so.
   * @private
   */
  reloadMenuForSelectionChange_() {
    let newSelectionState = this.nodeHasSelection_();
    if (this.selectionExists_ != newSelectionState) {
      this.selectionExists_ = newSelectionState;
      if (this.menuOriginNode_ &&
          !this.textNavigationManager_.currentlySelecting()) {
        let currentMenuId = this.menuPanel_.currentMenuId();
        if (currentMenuId) {
          this.openMenu_(this.menuOriginNode_, currentMenuId);
        } else {
          this.openMenu_(this.menuOriginNode_, SAConstants.MenuId.MAIN);
        }
      }
    }
  }

  /**
   * Determines which menu actions are relevant, given the current node. If
   * there are no node-specific actions, return |null|, to indicate that we
   * should select the current node automatically.
   *
   * @param {!SAChildNode} node
   * @return {Array<!SAConstants.MenuAction>}
   * @private
   */
  getMainMenuActionsForNode_(node) {
    let actions = node.actions;

    // Add text editing and navigation options.
    // TODO(anastasi): Move these actions into the node.
    const autoNode = node.automationNode;
    if (autoNode && SwitchAccess.get().improvedTextInputEnabled() &&
        SwitchAccessPredicate.isTextInput(autoNode) &&
        autoNode.state[StateType.FOCUSED]) {
      actions.push(SAConstants.MenuAction.MOVE_CURSOR);
      actions.push(SAConstants.MenuAction.SELECT_START);
      if (this.textNavigationManager_.currentlySelecting()) {
        actions.push(SAConstants.MenuAction.SELECT_END);
      }
      if (this.selectionExists_) {
        actions.push(SAConstants.MenuAction.CUT);
        actions.push(SAConstants.MenuAction.COPY);
      }
      if (this.clipboardHasData_) {
        actions.push(SAConstants.MenuAction.PASTE);
      }
    }

    // If there is at most one available action, perform it by default.
    if (actions.length <= 1) {
      return null;
    }


    // Add global actions.
    actions.push(SAConstants.MenuAction.SETTINGS);
    return actions;
  }

  /**
   * Perform a specified action on the Switch Access menu.
   * @param {!SAConstants.MenuAction} action
   */
  performAction(action) {
    SwitchAccessMetrics.recordMenuAction(action);
    // Some actions involve navigation events. Handle those explicitly.
    if (action === SAConstants.MenuAction.SELECT &&
        this.menuOriginNode_.isGroup()) {
      this.navigationManager_.enterGroup();
      this.exit();
      return;
    }
    if (action === SAConstants.MenuAction.OPEN_KEYBOARD) {
      this.navigationManager_.enterKeyboard();
      this.exit();
      return;
    }

    // Handle global actions.
    if (action === SAConstants.MenuAction.SETTINGS) {
      chrome.accessibilityPrivate.openSettingsSubpage(
          'manageAccessibility/switchAccess');
      this.exit();
      return;
    }

    // Handle text editing actions.
    // TODO(anastasi): Move these actions into the nodes themselves.
    switch (action) {
      case SAConstants.MenuAction.MOVE_CURSOR:
        if (this.menuOriginNode_) {
          this.openMenu_(
              this.menuOriginNode_, SAConstants.MenuId.TEXT_NAVIGATION,
              true /** Opening a submenu. */);
        }
        return;
      case SAConstants.MenuAction.JUMP_TO_BEGINNING_OF_TEXT:
        this.textNavigationManager_.jumpToBeginning();
        return;
      case SAConstants.MenuAction.JUMP_TO_END_OF_TEXT:
        this.textNavigationManager_.jumpToEnd();
        return;
      case SAConstants.MenuAction.MOVE_BACKWARD_ONE_CHAR_OF_TEXT:
        this.textNavigationManager_.moveBackwardOneChar();
        return;
      case SAConstants.MenuAction.MOVE_BACKWARD_ONE_WORD_OF_TEXT:
        this.textNavigationManager_.moveBackwardOneWord();
        return;
      case SAConstants.MenuAction.MOVE_DOWN_ONE_LINE_OF_TEXT:
        this.textNavigationManager_.moveDownOneLine();
        return;
      case SAConstants.MenuAction.MOVE_FORWARD_ONE_CHAR_OF_TEXT:
        this.textNavigationManager_.moveForwardOneChar();
        return;
      case SAConstants.MenuAction.MOVE_FORWARD_ONE_WORD_OF_TEXT:
        this.textNavigationManager_.moveForwardOneWord();
        return;
      case SAConstants.MenuAction.MOVE_UP_ONE_LINE_OF_TEXT:
        this.textNavigationManager_.moveUpOneLine();
        return;
      case SAConstants.MenuAction.CUT:
        EventHelper.simulateKeyPress(EventHelper.KeyCode.X, {ctrl: true});
        return;
      case SAConstants.MenuAction.COPY:
        EventHelper.simulateKeyPress(EventHelper.KeyCode.C, {ctrl: true});
        return;
      case SAConstants.MenuAction.PASTE:
        EventHelper.simulateKeyPress(EventHelper.KeyCode.V, {ctrl: true});
        return;
      case SAConstants.MenuAction.SELECT_START:
        this.textNavigationManager_.saveSelectStart();
        if (this.menuOriginNode_) {
          this.openMenu_(this.menuOriginNode_, SAConstants.MenuId.MAIN);
        }
        return;
      case SAConstants.MenuAction.SELECT_END:
        this.textNavigationManager_.resetCurrentlySelecting();
        if (this.menuOriginNode_) {
          this.openMenu_(this.menuOriginNode_, SAConstants.MenuId.MAIN);
        }
        return;
    }

    // Otherwise, ask the node to perform the action itself.
    if (this.menuOriginNode_.performAction(action)) {
      this.exit();
    }
  }

  /**
   * Send a message to the menu to update the focus ring around the current
   * node.
   * TODO(anastasi): Use real focus rings in the menu
   * @private
   * @param {boolean=} opt_clear If true, will clear the focus ring.
   */
  updateFocusRing_(opt_clear) {
    if (!this.menuPanel_) {
      console.log('Error: Menu panel has not loaded.');
      return;
    }

    if (!this.inMenu_ || !this.node_) {
      return;
    }
    let id = this.node_.automationNode.htmlAttributes.id;

    // If the selection will close the menu, highlight the back button.
    if (id === this.menuPanel_.currentMenuId()) {
      id = SAConstants.BACK_ID;
    }

    const enable = !opt_clear;
    this.menuPanel_.setFocusRing(id, enable);
  }
}
