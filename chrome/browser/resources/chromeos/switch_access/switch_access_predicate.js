// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const StateType = chrome.automation.StateType;
const RoleType = chrome.automation.RoleType;
const DefaultActionVerb = chrome.automation.DefaultActionVerb;

/**
 * Contains predicates for the chrome automation API. Each predicate can be run
 * on one or more AutomationNodes and returns a boolean value.
 */
const SwitchAccessPredicate = {
  /**
   * Returns a Restrictions object ready to be passed to AutomationTreeWalker.
   *
   * @param {!chrome.automation.AutomationNode} scope
   * @return {!AutomationTreeWalkerRestriction}
   */
  restrictions: (scope) => {
    return {
      leaf: SwitchAccessPredicate.leaf(scope),
      root: SwitchAccessPredicate.root(scope),
      visit: SwitchAccessPredicate.visit(scope)
    };
  },

  /**
   * Creates a function that confirms if |node| is a terminal leaf node of a
   * SwitchAccess scope tree when |scope| is the root.
   *
   * @param {!chrome.automation.AutomationNode} scope
   * @return {function(!chrome.automation.AutomationNode): boolean}
   */
  leaf: function(scope) {
    return (node) => node.state[StateType.INVISIBLE] ||
        (node !== scope && SwitchAccessPredicate.isSubtreeLeaf(node, scope)) ||
        !SwitchAccessPredicate.isInterestingSubtree(node);
  },

  /**
   * Creates a function that confirms if |node| is the root of a SwitchAccess
   * scope tree when |scope| is the root.
   *
   * @param {!chrome.automation.AutomationNode} scope
   * @return {function(!chrome.automation.AutomationNode): boolean}
   */
  root: function(scope) {
    return (node) => node === scope;
  },

  /**
   * Creates a function that determines whether |node| is to be visited in the
   * SwitchAccess scope tree with |scope| as the root.
   *
   * @param {!chrome.automation.AutomationNode} scope
   * @return {function(!chrome.automation.AutomationNode): boolean}
   */
  visit: function(scope) {
    return (node) => node.role !== RoleType.DESKTOP &&
        SwitchAccessPredicate.isSubtreeLeaf(node, scope);
  },

  /**
   * Returns a Restrictions object for finding the Context Menu root.
   * @return {!AutomationTreeWalkerRestriction}
   */
  contextMenuDiscoveryRestrictions: () => {
    return {
      leaf: SwitchAccessPredicate.isNotContainer,
      visit: SwitchAccessPredicate.isContextMenu
    };
  },

  /**
   * Returns true if |node| does not have a role of desktop, window, web view,
   * or root web area.
   * @param {!chrome.automation.AutomationNode} node
   * @return {boolean}
   */
  isNotContainer: (node) => node.role !== RoleType.ROOT_WEB_AREA &&
      node.role !== RoleType.WINDOW && node.role !== RoleType.DESKTOP &&
      node.role !== RoleType.WEB_VIEW,

  /**
   * Returns true if |node| is the context menu.
   * @param {!chrome.automation.AutomationNode} node
   * @return {boolean}
   */
  isContextMenu: (node) => node.htmlAttributes.id === ContextMenuManager.MenuId,

  /**
   * Returns true if |node| is a subtreeLeaf, meaning that |node|
   * is either interesting or a group (both defined below).
   *
   * @param {!chrome.automation.AutomationNode} node
   * @param {!chrome.automation.AutomationNode} scope
   * @return {boolean}
   */
  isSubtreeLeaf: (node, scope) => SwitchAccessPredicate.isActionable(node) ||
      SwitchAccessPredicate.isGroup(node, scope),

  /**
   * Returns true if |node| is a group, meaning that the node has more than one
   * interesting descendant, and that its interesting descendants exist in more
   * than one subtree of its immediate children.
   *
   * Additionally, for |node| to be a group, it cannot have the same bounding
   * box as its scope.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @param {!chrome.automation.AutomationNode} scope
   * @return {boolean}
   */
  isGroup: (node, scope) => {
    if (node !== scope && SwitchAccessPredicate.hasSameLocation_(node, scope))
      return false;
    if (node.state[StateType.INVISIBLE])
      return false;

    // Work around for client nested in client. No need to have user select both
    // clients for a window. Once locations for outer client updates correctly,
    // this won't be needed.
    if (node.role === RoleType.CLIENT && node.role === scope.role &&
        node !== scope)
      return false;

    let interestingBranchesCount =
        SwitchAccessPredicate.isActionable(node) ? 1 : 0;
    let child = node.firstChild;
    while (child) {
      if (SwitchAccessPredicate.isInterestingSubtree(child))
        interestingBranchesCount += 1;
      if (interestingBranchesCount > 1)
        return true;
      child = child.nextSibling;
    }
    return false;
  },

  /**
   * Returns true if there is an interesting node in the subtree containing
   * |node| as its root (including |node| itself).
   *
   * @param {!chrome.automation.AutomationNode} node
   * @return {boolean}
   */
  isInterestingSubtree: (node) => SwitchAccessPredicate.isActionable(node) ||
      node.children.some(SwitchAccessPredicate.isInterestingSubtree),

  /**
   * Returns true if |node| is interesting, meaning that a user can perform some
   * type of action on it.
   *
   * @param {!chrome.automation.AutomationNode} node
   * @return {boolean}
   */
  isActionable: (node) => {
    const defaultActionVerb = node.defaultActionVerb;
    const loc = node.location;
    const parent = node.parent;
    const root = node.root;
    const role = node.role;
    const state = node.state;

    // Skip things that are offscreen or invisible.
    if (state[StateType.OFFSCREEN] || loc.top < 0 || loc.left < 0 ||
        state[StateType.INVISIBLE])
      return false;

    // Skip things that are disabled.
    if (node.restriction === chrome.automation.Restriction.DISABLED)
      return false;

    // These web containers are not directly actionable.
    if (role === RoleType.WEB_VIEW || role === RoleType.ROOT_WEB_AREA)
      return false;

    if (parent) {
      // crbug.com/710559
      // Work around for browser tabs.
      if (role === RoleType.TAB && parent.role === RoleType.TAB_LIST &&
          root.role === RoleType.DESKTOP)
        return true;
    }

    // Check various indicators that the node is actionable.
    // TODO(zhelfins): Update tests to reflect this updated behavior.

    if (defaultActionVerb && defaultActionVerb !== 'none' &&
        defaultActionVerb !== DefaultActionVerb.CLICK_ANCESTOR)
      return true;

    if (node.inputType)
      return true;

    if (role === RoleType.BUTTON)
      return true;

    return false;
  },

  /**
   * Returns true if the two nodes have the same location.
   *
   * @param {!chrome.automation.AutomationNode} node1
   * @param {!chrome.automation.AutomationNode} node2
   * @return {boolean}
   */
  hasSameLocation_: (node1, node2) => {
    const l1 = node1.location;
    const l2 = node2.location;
    return l1.left === l2.left && l1.top === l2.top && l1.width === l2.width &&
        l1.height === l2.height;
  }
};
