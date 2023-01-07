// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationPredicate} from '../common/automation_predicate.js';
import {RectUtil} from '../common/rect_util.js';
import {AutomationTreeWalkerRestriction} from '../common/tree_walker.js';

import {SACache} from './cache.js';
import {SAChildNode, SARootNode} from './nodes/switch_access_node.js';

const AutomationNode = chrome.automation.AutomationNode;
const StateType = chrome.automation.StateType;
const RoleType = chrome.automation.RoleType;
const DefaultActionVerb = chrome.automation.DefaultActionVerb;

/**
 * Contains predicates for the chrome automation API. The following basic
 * predicates are available:
 *    - isActionable
 *    - isGroup
 *    - isInteresting
 *    - isInterestingSubtree
 *    - isVisible
 *    - isTextInput
 *
 * In addition to these basic predicates, there are also methods to get the
 * restrictions required by TreeWalker for specific traversal situations.
 */
export const SwitchAccessPredicate = {
  GROUP_INTERESTING_CHILD_THRESHOLD: 2,

  /**
   * Returns true if |node| is actionable, meaning that a user can interact with
   * it in some way.
   *
   * @param {!AutomationNode} node
   * @param {!SACache} cache
   * @return {boolean}
   */
  isActionable: (node, cache) => {
    if (cache.isActionable.has(node)) {
      return cache.isActionable.get(node);
    }

    const defaultActionVerb = node.defaultActionVerb;
    const loc = node.location;
    const parent = node.parent;
    const role = node.role;
    const state = node.state;

    // Skip things that are offscreen or invisible.
    if (!SwitchAccessPredicate.isVisible(node)) {
      cache.isActionable.set(node, false);
      return false;
    }

    // Skip things that are disabled.
    if (node.restriction === chrome.automation.Restriction.DISABLED) {
      cache.isActionable.set(node, false);
      return false;
    }

    // These web containers are not directly actionable.
    if (role === RoleType.WEB_VIEW || role === RoleType.ROOT_WEB_AREA) {
      cache.isActionable.set(node, false);
      return false;
    }

    // Check various indicators that the node is actionable.
    if (role === RoleType.BUTTON || role === RoleType.SLIDER ||
        role === RoleType.TAB) {
      cache.isActionable.set(node, true);
      return true;
    }

    if (AutomationPredicate.comboBox(node) ||
        SwitchAccessPredicate.isTextInput(node)) {
      cache.isActionable.set(node, true);
      return true;
    }

    if (defaultActionVerb &&
        (defaultActionVerb === DefaultActionVerb.ACTIVATE ||
         defaultActionVerb === DefaultActionVerb.CHECK ||
         defaultActionVerb === DefaultActionVerb.OPEN ||
         defaultActionVerb === DefaultActionVerb.PRESS ||
         defaultActionVerb === DefaultActionVerb.SELECT ||
         defaultActionVerb === DefaultActionVerb.UNCHECK)) {
      cache.isActionable.set(node, true);
      return true;
    }

    if (role === RoleType.LIST_ITEM &&
        defaultActionVerb === DefaultActionVerb.CLICK) {
      cache.isActionable.set(node, true);
      return true;
    }

    // Focusable items should be surfaced as either groups or actionable. So
    // should menu items.
    // Current heuristic is to show as actionble any focusable item where no
    // child is an interesting subtree.
    if (state[StateType.FOCUSABLE] || role === RoleType.MENU_ITEM) {
      const result = !node.children.some(
          child => SwitchAccessPredicate.isInterestingSubtree(child, cache));
      cache.isActionable.set(node, result);
      return result;
    }
    return false;
  },

  /**
   * Returns true if |node| is a group, meaning that the node has more than one
   * interesting descendant, and that its interesting descendants exist in more
   * than one subtree of its immediate children.
   *
   * Additionally, for |node| to be a group, it cannot have the same bounding
   * box as its scope.
   *
   * @param {!AutomationNode} node
   * @param {!AutomationNode|!SARootNode|null} scope
   * @param {!SACache} cache
   * @return {boolean}
   */
  isGroup: (node, scope, cache) => {
    if (cache.isGroup.has(node)) {
      return cache.isGroup.get(node);
    }

    const scopeEqualsNode = scope &&
        (scope instanceof SARootNode ? scope.isEquivalentTo(node) :
                                       scope === node);
    if (scope && !scopeEqualsNode &&
        RectUtil.equal(node.location, scope.location)) {
      cache.isGroup.set(node, false);
      return false;
    }
    if (node.state[StateType.INVISIBLE]) {
      cache.isGroup.set(node, false);
      return false;
    }

    if (node.role === chrome.automation.RoleType.KEYBOARD) {
      cache.isGroup.set(node, true);
      return true;
    }

    let interestingBranchesCount =
        SwitchAccessPredicate.isActionable(node, cache) ? 1 : 0;
    let child = node.firstChild;
    while (child) {
      if (SwitchAccessPredicate.isInterestingSubtree(child, cache)) {
        interestingBranchesCount += 1;
      }
      if (interestingBranchesCount >=
          SwitchAccessPredicate.GROUP_INTERESTING_CHILD_THRESHOLD) {
        cache.isGroup.set(node, true);
        return true;
      }
      child = child.nextSibling;
    }
    cache.isGroup.set(node, false);
    return false;
  },

  /**
   * Returns true if |node| is interesting for the user, meaning that |node|
   * is either actionable or a group.
   *
   * @param {!AutomationNode} node
   * @param {!AutomationNode|!SARootNode} scope
   * @param {!SACache} cache
   * @return {boolean}
   */
  isInteresting: (node, scope, cache) => {
    cache = cache || new SACache();
    return SwitchAccessPredicate.isActionable(node, cache) ||
        SwitchAccessPredicate.isGroup(node, scope, cache);
  },

  /**
   * Returns true if the element is visible to the user for any reason.
   *
   * @param {AutomationNode} node
   * @return {boolean}
   */
  isVisible: node => Boolean(
      !node.state[StateType.OFFSCREEN] && node.location &&
      node.location.top >= 0 && node.location.left >= 0 &&
      !node.state[StateType.INVISIBLE]),

  /**
   * Returns true if there is an interesting node in the subtree containing
   * |node| as its root (including |node| itself).
   *
   * This function does not call isInteresting directly, because that would
   * cause a loop (isInteresting calls isGroup, and isGroup calls
   * isInterestingSubtree).
   *
   * @param {!AutomationNode} node
   * @param {!SACache} cache
   * @return {boolean}
   */
  isInterestingSubtree: (node, cache) => {
    cache = cache || new SACache();
    if (cache.isInterestingSubtree.has(node)) {
      return cache.isInterestingSubtree.get(node);
    }
    const result = SwitchAccessPredicate.isActionable(node, cache) ||
        node.children.some(
            child => SwitchAccessPredicate.isInterestingSubtree(child, cache));
    cache.isInterestingSubtree.set(node, result);
    return result;
  },

  /**
   * Returns true if |node| is an element that contains editable text.
   * @param {AutomationNode} node
   * @return {boolean}
   */
  isTextInput: node => Boolean(node && node.state[StateType.EDITABLE]),

  /**
   * Returns true if |node| should be considered a window.
   * @param {AutomationNode} node
   * @return {boolean}
   */
  isWindow: node => Boolean(
      node &&
      (node.role === chrome.automation.RoleType.WINDOW ||
       (node.role === chrome.automation.RoleType.CLIENT && node.parent &&
        node.parent.role === chrome.automation.RoleType.WINDOW))),

  /**
   * Returns a Restrictions object ready to be passed to AutomationTreeWalker.
   *
   * @param {!AutomationNode} scope
   * @return {!AutomationTreeWalkerRestriction}
   */
  restrictions: scope => {
    const cache = new SACache();
    return {
      leaf: SwitchAccessPredicate.leaf(scope, cache),
      root: SwitchAccessPredicate.root(scope),
      visit: SwitchAccessPredicate.visit(scope, cache),
    };
  },

  /**
   * Creates a function that confirms if |node| is a terminal leaf node of a
   * SwitchAccess scope tree when |scope| is the root.
   *
   * @param {!AutomationNode} scope
   * @param {!SACache} cache
   * @return {function(!AutomationNode): boolean}
   */
  leaf(scope, cache) {
    return node => !SwitchAccessPredicate.isInterestingSubtree(node, cache) ||
        (scope !== node &&
         SwitchAccessPredicate.isInteresting(node, scope, cache));
  },

  /**
   * Creates a function that confirms if |node| is the root of a SwitchAccess
   * scope tree when |scope| is the root.
   *
   * @param {!AutomationNode} scope
   * @return {function(!AutomationNode): boolean}
   */
  root(scope) {
    return node => scope === node;
  },

  /**
   * Creates a function that determines whether |node| is to be visited in the
   * SwitchAccess scope tree with |scope| as the root.
   *
   * @param {!AutomationNode} scope
   * @param {!SACache} cache
   * @return {function(!AutomationNode): boolean}
   */
  visit(scope, cache) {
    return node => node.role !== RoleType.DESKTOP &&
        SwitchAccessPredicate.isInteresting(node, scope, cache);
  },
};
