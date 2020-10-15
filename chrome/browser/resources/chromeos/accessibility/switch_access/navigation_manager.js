// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** This class handles navigation amongst the elements onscreen. */
class NavigationManager {
  /**
   * @param {!AutomationNode} desktop
   * @private
   */
  constructor(desktop) {
    /** @private {!AutomationNode} */
    this.desktop_ = desktop;

    /** @private {!SARootNode} */
    this.group_ = DesktopNode.build(this.desktop_);

    /** @private {!SAChildNode} */
    // TODO(crbug.com/1106080): It is possible for the firstChild to be a
    // window which is occluded, for example if Switch Access is turned on
    // when the user has several browser windows opened. We should either
    // dynamically pick this.node_'s initial value based on an occlusion check,
    // or ensure that we move away from occluded children as quickly as soon
    // as they are detected using an interval set in DesktopNode.
    this.node_ = this.group_.firstChild;

    /** @private {!FocusHistory} */
    this.history_ = new FocusHistory();

    this.init_();
  }

  // =============== Static Methods ==============

  /**
   * @param {!SAChildNode} node
   * @return {boolean}
   */
  static currentGroupHasChild(node) {
    return NavigationManager.instance.group_.children.includes(node);
  }

  /**
   * Enters |this.node_|.
   */
  static enterGroup() {
    const navigator = NavigationManager.instance;
    if (!navigator.node_.isGroup()) {
      return;
    }

    const newGroup = navigator.node_.asRootNode();
    if (newGroup) {
      navigator.history_.save(new FocusData(navigator.group_, navigator.node_));
      navigator.setGroup_(newGroup);
    }
  }

  /**
   * Puts focus on the virtual keyboard, if the current node is a text input.
   */
  static enterKeyboard() {
    const navigator = NavigationManager.instance;
    navigator.node_.automationNode.focus();
    const keyboard = KeyboardRootNode.buildTree();
    navigator.jumpTo_(keyboard);
  }

  /** Unconditionally exits the current group. */
  static exitGroupUnconditionally() {
    NavigationManager.instance.exitGroup_();
  }

  /**
   * Exits the specified node, if it is the currently focused group.
   * @param {?AutomationNode|!SAChildNode|!SARootNode} node
   */
  static exitIfInGroup(node) {
    const navigator = NavigationManager.instance;
    if (navigator.group_.isEquivalentTo(node)) {
      navigator.exitGroup_();
    }
  }

  static exitKeyboard() {
    const navigator = NavigationManager.instance;
    const isKeyboard = (data) => data.group instanceof KeyboardRootNode;
    // If we are not in the keyboard, do nothing.
    if (!(navigator.group_ instanceof KeyboardRootNode) &&
        !navigator.history_.containsDataMatchingPredicate(isKeyboard)) {
      return;
    }

    while (navigator.history_.peek() !== null) {
      if (navigator.group_ instanceof KeyboardRootNode) {
        navigator.exitGroup_();
        break;
      }
      navigator.exitGroup_();
    }

    NavigationManager.moveToValidNode();
  }

  /**
   * Forces the current node to be |node|.
   * Should only be called by subclasses of SARootNode and
   *    only when they are focused.
   * @param {!SAChildNode} node
   */
  static forceFocusedNode(node) {
    const navigator = NavigationManager.instance;
    // Check if they are exactly the same instance. Checking contents
    // equality is not sufficient in case the node has been repopulated
    // after a refresh.
    if (navigator.node_ !== node) {
      navigator.setNode_(node);
    }
  }

  /**
   * Returns the current Switch Access tree, for debugging purposes.
   * @param {boolean} wholeTree Whether to print the whole tree, or just the
   * current focus.
   * @return {!SARootNode}
   */
  static getTreeForDebugging(wholeTree) {
    if (!wholeTree) {
      console.log(NavigationManager.instance.group_.debugString(wholeTree));
      return NavigationManager.instance.group_;
    }

    const desktopRoot = DesktopNode.build(NavigationManager.instance.desktop_);
    console.log(desktopRoot.debugString(
        wholeTree, '', NavigationManager.instance.node_));
    return desktopRoot;
  }

  /** @param {!AutomationNode} desktop */
  static initialize(desktop) {
    NavigationManager.instance = new NavigationManager(desktop);
  }

  /** @param {AutomationNode} menuNode */
  static jumpToSwitchAccessMenu(menuNode) {
    if (!menuNode) {
      return;
    }
    const menu = BasicRootNode.buildTree(menuNode);
    NavigationManager.instance.jumpTo_(menu, false /* shouldExitMenu */);
  }

  /**
   * Move to the previous interesting node.
   */
  static moveBackward() {
    const navigator = NavigationManager.instance;
    if (navigator.node_.isValidAndVisible()) {
      NavigationManager.tryMoving(
          navigator.node_.previous, (node) => node.previous, navigator.node_);
    } else {
      NavigationManager.moveToValidNode();
    }
  }

  /**
   * Move to the next interesting node.
   */
  static moveForward() {
    const navigator = NavigationManager.instance;
    if (navigator.node_.isValidAndVisible()) {
      NavigationManager.tryMoving(
          navigator.node_.next, (node) => node.next, navigator.node_);
    } else {
      NavigationManager.moveToValidNode();
    }
  }

  /**
   * Tries to move to another node, |node|, but if |node| is a window that's not
   * in the foreground it will use |getNext| to find the next node to try.
   * Checks against |startingNode| to ensure we don't get stuck in an infinite
   * loop.
   * @param {!SAChildNode} node The node to try to move into.
   * @param {function(!SAChildNode): !SAChildNode} getNext gets the next node to
   *     try if we cannot move to |next|. Takes |next| as a parameter.
   * @param {!SAChildNode} startingNode The first node in the sequence. If we
   *     loop back to this node, stop trying to move, as there are no other
   *     nodes we can move to.
   */
  static tryMoving(node, getNext, startingNode) {
    if (node == startingNode) {
      // This should only happen if the desktop contains exactly one interesting
      // child and all other children are windows which are occluded.
      // Unlikely to happen since we can always access the shelf.
      return;
    }
    const navigator = NavigationManager.instance;
    if (!(node instanceof BasicNode)) {
      navigator.setNode_(node);
      return;
    }
    if (!SwitchAccessPredicate.isWindow(node.automationNode)) {
      navigator.setNode_(node);
      return;
    }
    const location = node.location;
    if (!location) {
      // Closure compiler doesn't realize we already checked isValidAndVisible
      // before calling tryMoving, so we need to explicitly check location here
      // so that RectUtil.center does not cause a closure error.
      NavigationManager.moveToValidNode();
      return;
    }
    const center = RectUtil.center(location);
    // Check if the top center is visible as a proxy for occlusion. It's
    // possible that other parts of the window are occluded, but in Chrome we
    // can't drag windows off the top of the screen.
    navigator.desktop_.hitTestWithReply(center.x, location.top, (hitNode) => {
      if (AutomationUtil.isDescendantOf(hitNode, node.automationNode)) {
        navigator.setNode_(node);
      } else if (node.isValidAndVisible()) {
        NavigationManager.tryMoving(getNext(node), getNext, startingNode);
      } else {
        NavigationManager.moveToValidNode();
      }
    });
  }

  /**
   * Moves to the Switch Access focus up the group stack closest to the ancestor
   * that hasn't been invalidated.
   */
  static moveToValidNode() {
    const navigator = NavigationManager.instance;

    const nodeIsValid = navigator.node_.isValidAndVisible();
    const groupIsValid = navigator.group_.isValidGroup();

    if (nodeIsValid && groupIsValid) {
      return;
    }

    if (nodeIsValid && !(navigator.node_ instanceof BackButtonNode)) {
      // Our group has been invalidated. Move to navigator node to repair the
      // group stack.
      navigator.moveTo_(navigator.node_.automationNode);
      return;
    }

    // Make sure the menu isn't open.
    MenuManager.exit();

    const child = navigator.group_.firstValidChild();
    if (groupIsValid && child) {
      navigator.setNode_(child);
      return;
    }

    navigator.restoreFromHistory_();
  }

  // =============== Getter Methods ==============

  /**
   * Returns the currently focused node.
   * @return {!SAChildNode}
   */
  static get currentNode() {
    NavigationManager.moveToValidNode();
    return NavigationManager.instance.node_;
  }

  /**
   * Returns the desktop automation node object.
   * @return {!AutomationNode}
   */
  static get desktopNode() {
    return NavigationManager.instance.desktop_;
  }

  // =============== Event Handlers ==============

  /**
   * When focus shifts, move to the element. Find the closest interesting
   *     element to engage with.
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onFocusChange_(event) {
    // Ignore focus changes from our own actions.
    if (event.eventFrom == 'action') {
      return;
    }

    if (this.node_.isEquivalentTo(event.target)) {
      return;
    }
    this.moveTo_(event.target);
  }

  /**
   * When scroll position changes, ensure that the focus ring is in the
   * correct place and that the focused node / node group are valid.
   * @private
   */
  onScrollChange_() {
    if (this.node_.isValidAndVisible()) {
      // Update focus ring.
      FocusRingManager.setFocusedNode(this.node_);
    }
    this.group_.refresh();
    MenuManager.refreshMenu();
  }

  /**
   * When a menu is opened, jump focus to the menu.
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onModalDialog_(event) {
    const modalRoot = ModalDialogRootNode.buildTree(event.target);
    if (modalRoot.isValidGroup()) {
      this.jumpTo_(modalRoot);
    }
  }

  /**
   * When the automation tree changes, ensure the group and node we are
   * currently listening to are fresh. This is only called when the tree change
   * occurred on the node or group which are currently active.
   * @param {!chrome.automation.TreeChange} treeChange
   * @private
   */
  onTreeChange_(treeChange) {
    if (treeChange.type === chrome.automation.TreeChangeType.NODE_REMOVED) {
      this.group_.refresh();
      NavigationManager.moveToValidNode();
    } else if (
        treeChange.type ===
        chrome.automation.TreeChangeType.SUBTREE_UPDATE_END) {
      this.group_.refresh();
    }
  }

  // =============== Private Methods ==============

  /** @private */
  exitGroup_() {
    this.group_.onExit();
    this.restoreFromHistory_();
  }

  /** @private */
  init_() {
    this.group_.onFocus();
    this.node_.onFocus();

    new RepeatedEventHandler(
        this.desktop_, chrome.automation.EventType.FOCUS,
        this.onFocusChange_.bind(this));

    // ARC++ fires SCROLL_POSITION_CHANGED.
    new RepeatedEventHandler(
        this.desktop_, chrome.automation.EventType.SCROLL_POSITION_CHANGED,
        this.onScrollChange_.bind(this));

    // Web and Views use AXEventGenerator, which fires
    // separate horizontal and vertical events.
    new RepeatedEventHandler(
        this.desktop_,
        chrome.automation.EventType.SCROLL_HORIZONTAL_POSITION_CHANGED,
        this.onScrollChange_.bind(this));
    new RepeatedEventHandler(
        this.desktop_,
        chrome.automation.EventType.SCROLL_VERTICAL_POSITION_CHANGED,
        this.onScrollChange_.bind(this));

    new RepeatedTreeChangeHandler(
        chrome.automation.TreeChangeObserverFilter.ALL_TREE_CHANGES,
        this.onTreeChange_.bind(this), {
          predicate: (treeChange) =>
              this.group_.findChild(treeChange.target) != null ||
              this.group_.isEquivalentTo(treeChange.target)
        });

    // The status tray fires a SHOW event when it opens.
    new EventHandler(
        this.desktop_,
        [
          chrome.automation.EventType.MENU_START,
          chrome.automation.EventType.SHOW
        ],
        this.onModalDialog_.bind(this))
        .start();
  }

  /**
   * Jumps Switch Access focus to a specified node, such as when opening a menu
   * or the keyboard. Does not modify the groups already in the group stack.
   * @param {!SARootNode} group
   * @param {boolean} shouldExitMenu
   * @private
   */
  jumpTo_(group, shouldExitMenu = true) {
    if (shouldExitMenu) {
      MenuManager.exit();
    }

    this.history_.save(new FocusData(this.group_, this.node_));
    this.setGroup_(group);
  }

  /**
   * Moves Switch Access focus to a specified node, based on a focus shift or
   *     tree change event. Reconstructs the group stack to center on that node.
   *
   * This is a "permanent" move, while |jumpTo_| is a "temporary" move.
   *
   * @param {!AutomationNode} automationNode
   * @private
   */
  moveTo_(automationNode) {
    MenuManager.exit();
    if (this.history_.buildFromAutomationNode(automationNode)) {
      this.restoreFromHistory_();
    }
  }

  /**
   * Restores the most proximal state from the history.
   * @private
   */
  restoreFromHistory_() {
    const data = this.history_.retrieve();
    // retrieve() guarantees that the group is valid, but not the focus.
    if (data.focus.isValidAndVisible()) {
      this.setGroup_(data.group, data.focus);
    } else {
      this.setGroup_(data.group);
    }
  }

  /**
   * Set |this.group_| to |group|, and sets |this.node_| to either |opt_focus|
   * or |group.firstChild|.
   * @param {!SARootNode} group
   * @param {SAChildNode=} opt_focus
   * @private
   */
  setGroup_(group, opt_focus) {
    this.group_.onUnfocus();
    this.group_ = group;
    this.group_.onFocus();

    const node = opt_focus || this.group_.firstValidChild();
    if (!node) {
      NavigationManager.moveToValidNode();
      return;
    }
    this.setNode_(node);
  }

  /**
   * Set |this.node_| to |node|, and update what is displayed onscreen.
   * @param {!SAChildNode} node
   * @private
   */
  setNode_(node) {
    if (!node.isValidAndVisible()) {
      NavigationManager.moveToValidNode();
      return;
    }
    this.node_.onUnfocus();
    this.node_ = node;
    this.node_.onFocus();
    AutoScanManager.restartIfRunning();
  }
}
