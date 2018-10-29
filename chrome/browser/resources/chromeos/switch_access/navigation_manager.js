// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage interactions with the accessibility tree, including moving
 * to and selecting nodes.
 */
class NavigationManager {
  /**
   * @param {!chrome.automation.AutomationNode} desktop
   */
  constructor(desktop) {
    /**
     * Handles communication with and navigation within the context menu.
     */
    this.contextMenuManager_ = new ContextMenuManager(this, desktop);

    /**
     *
     * The desktop node.
     *
     * @private {!chrome.automation.AutomationNode}
     */
    this.desktop_ = desktop;

    /**
     * The currently highlighted node.
     *
     * @private {!chrome.automation.AutomationNode}
     */
    this.node_ = desktop;

    /**
     * The root of the subtree that the user is navigating through.
     *
     * @private {!chrome.automation.AutomationNode}
     */
    this.scope_ = desktop;

    /**
     * A stack of past scopes. Allows user to traverse back to previous groups
     * after selecting one or more groups. The most recent group is at the end
     * of the array.
     *
     * @private {Array<!chrome.automation.AutomationNode>}
     */
    this.scopeStack_ = [];

    /**
     * Keeps track of when we're visiting the current scope as an actionable
     * node.
     * @private {boolean}
     */
    this.visitingScopeAsActionable_ = false;

    this.init_();
  }

  /**
   * Open the context menu for the currently highlighted node.
   */
  enterContextMenu() {
    // If we're currently visiting the context menu, this command should select
    // the highlighted element.
    if (this.contextMenuManager_.selectCurrentNode())
      return;

    this.contextMenuManager_.enter(this.getRelevantMenuActions_());
  }

  /**
   * Find the previous interesting node and update |this.node_|. If there is no
   * previous node, |this.node_| will be set to the youngest descendant in the
   * SwitchAccess scope tree to loop again.
   */
  moveBackward() {
    if (this.contextMenuManager_.moveBackward())
      return;

    this.startAtValidNode_();

    let treeWalker = new AutomationTreeWalker(
        this.node_, constants.Dir.BACKWARD,
        SwitchAccessPredicate.restrictions(this.scope_));

    // Special case: Scope is actionable
    if (this.node_ === this.scope_ && this.visitingScopeAsActionable_) {
      this.visitingScopeAsActionable_ = false;
      this.setCurrentNode_(this.node_);
      return;
    }

    let node = treeWalker.next().node;

    // Special case: Scope is actionable
    if (node === this.scope_ && SwitchAccessPredicate.isActionable(node)) {
      this.showScopeAsActionable_();
      return;
    }

    // If treeWalker returns undefined, that means we're at the end of the tree
    // and we should start over.
    if (!node)
      node = this.youngestDescendant_(this.scope_);

    this.setCurrentNode_(node);
  }

  /**
   * Find the next interesting node, and update |this.node_|. If there is no
   * next node, |this.node_| will be set equal to |this.scope_| to loop again.
   */
  moveForward() {
    if (this.contextMenuManager_.moveForward())
      return;

    this.startAtValidNode_();

    let treeWalker = new AutomationTreeWalker(
        this.node_, constants.Dir.FORWARD,
        SwitchAccessPredicate.restrictions(this.scope_));

    // Special case: Scope is actionable.
    if (this.node_ === this.scope_ &&
        SwitchAccessPredicate.isActionable(this.node_) &&
        !this.visitingScopeAsActionable_) {
      this.showScopeAsActionable_();
      return;
    }
    this.visitingScopeAsActionable_ = false;

    let node = treeWalker.next().node;
    // If treeWalker returns undefined, that means we're at the end of the tree
    // and we should start over.
    if (!node)
      node = this.scope_;

    this.setCurrentNode_(node);
  }

  /**
   * Scrolls the current node in the direction indicated by |scrollAction|.
   * @param {!ContextMenuManager.Action} scrollAction
   */
  scroll(scrollAction) {
    // Find the closest ancestor to the current node that is scrollable.
    let scrollNode = this.node_;
    while (scrollNode && scrollNode.scrollX === undefined)
      scrollNode = scrollNode.parent;
    if (!scrollNode)
      return;

    if (scrollAction === ContextMenuManager.Action.SCROLL_DOWN)
      scrollNode.scrollDown(() => {});
    else if (scrollAction === ContextMenuManager.Action.SCROLL_UP)
      scrollNode.scrollUp(() => {});
    else if (scrollAction === ContextMenuManager.Action.SCROLL_LEFT)
      scrollNode.scrollLeft(() => {});
    else if (scrollAction === ContextMenuManager.Action.SCROLL_RIGHT)
      scrollNode.scrollRight(() => {});
    else if (scrollAction === ContextMenuManager.Action.SCROLL_FORWARD)
      scrollNode.scrollForward(() => {});
    else if (scrollAction === ContextMenuManager.Action.SCROLL_BACKWARD)
      scrollNode.scrollBackward(() => {});
    else
      console.log('Unrecognized scroll action: ', scrollAction);
  }

  /**
   * Perform the default action for the currently highlighted node. If the node
   * is the current scope, go back to the previous scope. If the node is a group
   * other than the current scope, go into that scope. If the node is
   * interesting, perform the default action on it.
   */
  selectCurrentNode() {
    if (this.contextMenuManager_.selectCurrentNode())
      return;

    if (!this.node_.role)
      return;

    if (this.node_ === this.scope_) {
      // If we're visiting the scope as actionable, perform the default action.
      if (this.visitingScopeAsActionable_) {
        this.node_.doDefault();
        return;
      }

      // Don't let user select the top-level root node (i.e., the desktop node).
      if (this.scopeStack_.length === 0)
        return;

      // Find a previous scope that is still valid. The stack here always has
      // at least one valid scope (i.e., the desktop node).
      do {
        this.scope_ = this.scopeStack_.pop();
      } while (!this.scope_.role && this.scopeStack_.length > 0);

      this.updateFocusRing_();
      return;
    }

    if (SwitchAccessPredicate.isGroup(this.node_, this.scope_)) {
      this.scopeStack_.push(this.scope_);
      this.scope_ = this.node_;
      this.moveForward();
      return;
    }

    this.node_.doDefault();
  }

  // ----------------------Private Methods---------------------

  /**
   * Create a new scope stack and set the current scope for |node|.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @private
   */
  buildScopeStack_(node) {
    // Create list of |node|'s ancestors, with highest level ancestor at the
    // end.
    let ancestorList = [];
    while (node.parent) {
      ancestorList.push(node.parent);
      node = node.parent;
    }

    // Starting with desktop as the scope, if an ancestor is a group, set it to
    // the new scope and push the old scope onto the scope stack.
    this.scopeStack_ = [];
    this.scope_ = this.desktop_;
    while (ancestorList.length > 0) {
      let ancestor = ancestorList.pop();
      if (ancestor.role === chrome.automation.RoleType.DESKTOP)
        continue;
      if (SwitchAccessPredicate.isGroup(ancestor, this.scope_)) {
        this.scopeStack_.push(this.scope_);
        this.scope_ = ancestor;
      }
    }
  }

  /**
   * Determines which menu actions are relevant, given the current node and
   * scope.
   * @private
   */
  getRelevantMenuActions_() {
    // TODO(crbug/881080): determine relevant actions programmatically.
    let actions = [
      ContextMenuManager.Action.CLICK, ContextMenuManager.Action.DICTATION,
      ContextMenuManager.Action.OPTIONS, ContextMenuManager.Action.SCROLL_UP,
      ContextMenuManager.Action.SCROLL_DOWN
    ];
    return actions;
  }

  /**
   * When an interesting element gains focus on the page, move to it. If an
   * element gains focus but is not interesting, move to the next interesting
   * node after it.
   *
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  handleFocusChange_(event) {
    if (this.node_ === event.target)
      return;

    // Rebuild scope stack and set scope for focused node.
    this.buildScopeStack_(event.target);

    // Move to focused node.
    this.node_ = event.target;

    // In case the node that gained focus is not a subtreeLeaf.
    if (SwitchAccessPredicate.isSubtreeLeaf(this.node_, this.scope_))
      this.updateFocusRing_();
    else
      this.moveForward();
  }

  /**
   * When a node is removed from the page, move to a new valid node.
   *
   * @param {!chrome.automation.TreeChange} treeChange
   * @private
   */
  handleNodeRemoved_(treeChange) {
    // TODO(elichtenberg): Only listen to NODE_REMOVED callbacks. Don't need
    // any others.
    if (treeChange.type !== chrome.automation.TreeChangeType.NODE_REMOVED)
      return;

    // TODO(elichtenberg): Currently not getting NODE_REMOVED event when whole
    // tree is deleted. Once fixed, can delete this. Should only need to check
    // if target is current node.
    let removedByRWA =
        treeChange.target.role === chrome.automation.RoleType.ROOT_WEB_AREA &&
        !this.node_.role;

    if (!removedByRWA && treeChange.target !== this.node_)
      return;

    chrome.accessibilityPrivate.setFocusRing([]);

    // Current node not invalid until after treeChange callback, so move to
    // valid node after callback. Delay added to prevent moving to another
    // node about to be made invalid. If already at a valid node (e.g., user
    // moves to it or focus changes to it), won't need to move to a new node.
    window.setTimeout(function() {
      if (!this.node_.role)
        this.moveForward();
    }.bind(this), 100);
  }

  /**
   * @private
   */
  init_() {
    this.desktop_.addEventListener(
        chrome.automation.EventType.FOCUS, this.handleFocusChange_.bind(this),
        false);

    // TODO(elichtenberg): Use a more specific filter than ALL_TREE_CHANGES.
    chrome.automation.addTreeChangeObserver(
        chrome.automation.TreeChangeObserverFilter.ALL_TREE_CHANGES,
        this.handleNodeRemoved_.bind(this));
  }

  /**
   * Set |this.node_| to |node|, and update its appearance onscreen.
   *
   * @param {!chrome.automation.AutomationNode} node
   */
  setCurrentNode_(node) {
    this.node_ = node;
    this.updateFocusRing_();
  }

  /**
   * Show the current scope as an actionable item.
   */
  showScopeAsActionable_() {
    this.node_ = this.scope_;
    this.visitingScopeAsActionable_ = true;

    this.updateFocusRing_(NavigationManager.Color.LEAF);
  }

  /**
   * Checks if this.node_ is valid. If so, do nothing.
   *
   * If this.node_ is not valid, set this.node_ to a valid scope. Will check the
   * current scope and past scopes until a valid scope is found. this.node_
   * is set to that valid scope.
   *
   * @private
   */
  startAtValidNode_() {
    if (this.node_.role)
      return;

    // Current node is invalid, but current scope is still valid, so set node
    // to the current scope.
    if (this.scope_.role)
      this.node_ = this.scope_;

    // Current node and current scope are invalid, so set both to a valid scope
    // from the scope stack. The stack here always has at least one valid scope
    // (i.e., the desktop node).
    while (!this.node_.role && this.scopeStack_.length > 0) {
      this.node_ = this.scopeStack_.pop();
      this.scope_ = this.node_;
    }
  }

  /**
   * Set the focus ring for the current node and determine the color for it.
   *
   * @param {NavigationManager.Color=} opt_color
   * @private
   */
  updateFocusRing_(opt_color) {
    let color;
    if (this.node_ === this.scope_)
      color = NavigationManager.Color.SCOPE;
    else if (SwitchAccessPredicate.isGroup(this.node_, this.scope_))
      color = NavigationManager.Color.GROUP;
    else
      color = NavigationManager.Color.LEAF;

    color = opt_color || color;
    chrome.accessibilityPrivate.setFocusRing([this.node_.location], color);
  }

  /**
   * Get the youngest descendant of |node|, if it has one within the current
   * scope.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @return {!chrome.automation.AutomationNode}
   * @private
   */
  youngestDescendant_(node) {
    const leaf = SwitchAccessPredicate.leaf(this.scope_);
    const visit = SwitchAccessPredicate.visit(this.scope_);

    const result = this.youngestDescendantHelper_(node, leaf, visit);
    if (!result)
      return this.scope_;
    return result;
  }

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @param {function(!chrome.automation.AutomationNode): boolean} leaf
   * @param {function(!chrome.automation.AutomationNode): boolean} visit
   * @return {chrome.automation.AutomationNode}
   * @private
   */
  youngestDescendantHelper_(node, leaf, visit) {
    if (!node)
      return null;

    if (leaf(node))
      return visit(node) ? node : null;

    const reverseChildren = node.children.reverse();
    for (const child of reverseChildren) {
      const youngest = this.youngestDescendantHelper_(child, leaf, visit);
      if (youngest)
        return youngest;
    }

    return visit(node) ? node : null;
  }

  // ----------------------Debugging Methods------------------------

  /**
   * Prints a debug version of the accessibility tree with annotations of
   * various SwitchAccess properties.
   *
   * To use, got to the console for SwitchAccess and run
   *    switchAccess.automationManager_.printDebugSwitchAccessTree()
   *
   * @param {NavigationManager.DisplayMode} opt_displayMode - an optional
   *     parameter that controls which nodes are printed. Default is
   *     INTERESTING_NODE.
   * @return {SwitchAccessDebugNode|undefined}
   */
  printDebugSwitchAccessTree(
      opt_displayMode = NavigationManager.DisplayMode.INTERESTING_NODE) {
    let allNodes = opt_displayMode === NavigationManager.DisplayMode.ALL;
    let debugRoot =
        NavigationManager.switchAccessDebugTree_(this.desktop_, allNodes);
    if (debugRoot)
      NavigationManager.printDebugNode_(debugRoot, 0, opt_displayMode);
    return debugRoot;
  }
  /**
   * creates a tree for debugging the SwitchAccess predicates, rooted at
   * node, based on the Accessibility tree.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @param {boolean} allNodes
   * @return {SwitchAccessDebugNode|undefined}
   * @private
   */
  static switchAccessDebugTree_(node, allNodes) {
    let debugNode = this.createAnnotatedDebugNode_(node, allNodes);
    if (!debugNode)
      return;

    for (let child of node.children) {
      let dChild = this.switchAccessDebugTree_(child, allNodes);
      if (dChild)
        debugNode.children.push(dChild);
    }
    return debugNode;
  }

  /**
   * Creates a debug node from the given automation node, with annotations of
   * various SwitchAccess properties.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @param {boolean} allNodes
   * @return {SwitchAccessDebugNode|undefined}
   * @private
   */
  static createAnnotatedDebugNode_(node, allNodes) {
    if (!allNodes && !SwitchAccessPredicate.isInterestingSubtree(node))
      return;

    let debugNode = {};
    if (node.role)
      debugNode.role = node.role;
    if (node.name)
      debugNode.name = node.name;

    debugNode.isActionable = SwitchAccessPredicate.isActionable(node);
    debugNode.isGroup = SwitchAccessPredicate.isGroup(node, node);
    debugNode.isInterestingSubtree =
        SwitchAccessPredicate.isInterestingSubtree(node);

    debugNode.children = [];
    debugNode.baseNode = node;
    return debugNode;
  }

  /**
   * Prints the debug subtree rooted at |node| in pre-order.
   *
   * @param {SwitchAccessDebugNode} node
   * @param {!number} indent
   * @param {NavigationManager.DisplayMode} displayMode
   * @private
   */
  static printDebugNode_(node, indent, displayMode) {
    if (!node)
      return;

    let result = ' '.repeat(indent);
    if (node.role)
      result += 'role:' + node.role + ' ';
    if (node.name)
      result += 'name:' + node.name + ' ';
    result += 'isActionable? ' + node.isActionable;
    result += ', isGroup? ' + node.isGroup;
    result += ', isInterestingSubtree? ' + node.isInterestingSubtree;

    switch (displayMode) {
      case NavigationManager.DisplayMode.ALL:
        console.log(result);
        break;
      case NavigationManager.DisplayMode.INTERESTING_SUBTREE:
        if (node.isInterestingSubtree)
          console.log(result);
        break;
      case NavigationManager.DisplayMode.INTERESTING_NODE:
      default:
        if (node.isActionable || node.isGroup)
          console.log(result);
        break;
    }

    let children = node.children || [];
    for (let child of children)
      this.printDebugNode_(child, indent + 2, displayMode);
  }
}

/**
 * Highlight colors for the focus ring to distinguish between different types
 * of nodes.
 *
 * @enum {string}
 * @const
 */
NavigationManager.Color = {
  SCOPE: '#de742f',  // dark orange
  GROUP: '#ffbb33',  // light orange
  LEAF: '#78e428'    // light green
};

/**
 * Display modes for debugging tree.
 *
 * @enum {string}
 * @const
 */
NavigationManager.DisplayMode = {
  ALL: 'all',
  INTERESTING_SUBTREE: 'interestingSubtree',
  INTERESTING_NODE: 'interestingNode'
};

/**
 * @typedef {{role: (string|undefined),
 *            name: (string|undefined),
 *            isActionable: boolean,
 *            isGroup: boolean,
 *            isInterestingSubtree: boolean,
 *            children: Array<SwitchAccessDebugNode>,
 *            baseNode: chrome.automation.AutomationNode}}
 */
let SwitchAccessDebugNode;
