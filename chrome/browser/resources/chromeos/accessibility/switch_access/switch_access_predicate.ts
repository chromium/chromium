// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationPredicate} from '/common/automation_predicate.js';
import {RectUtil} from '/common/rect_util.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';
import {AutomationTreeWalkerRestriction} from '/common/tree_walker.js';

import {SACache} from './cache.js';
import {SARootNode} from './nodes/switch_access_node.js';

type AutomationNode = chrome.automation.AutomationNode;
const StateType = chrome.automation.StateType;
const Restriction = chrome.automation.Restriction;
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
export namespace SwitchAccessPredicate {
  export const GROUP_INTERESTING_CHILD_THRESHOLD = 2;

  /**
   * Returns true if |node| is actionable, meaning that a user can interact with
   * it in some way.
   */
  export function isActionable(
      node: AutomationNode | undefined, cache: SACache): boolean {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (cache.isActionable.has(node!)) {
      return cache.isActionable.get(node!)!;
    }

    const defaultActionVerb = node!.defaultActionVerb;

    // Skip things that are offscreen or invisible.
    if (!SwitchAccessPredicate.isVisible(node!)) {
      cache.isActionable.set(node!, false);
      return false;
    }

    // Skip things that are disabled.
    if (node!.restriction === Restriction.DISABLED) {
      cache.isActionable.set(node!, false);
      return false;
    }

    // These web containers are not directly actionable.
    if (AutomationPredicate.structuralContainer(node!)) {
      cache.isActionable.set(node!, false);
      return false;
    }

    // Check various indicators that the node is actionable.
    const actionableRole = AutomationPredicate.roles(
        [RoleType.BUTTON, RoleType.SLIDER, RoleType.TAB]);
    if (actionableRole(node!)) {
      cache.isActionable.set(node!, true);
      return true;
    }

    if (AutomationPredicate.comboBox(node!) ||
        SwitchAccessPredicate.isTextInput(node!)) {
      cache.isActionable.set(node!, true);
      return true;
    }

    if (defaultActionVerb &&
        (defaultActionVerb === DefaultActionVerb.ACTIVATE ||
         defaultActionVerb === DefaultActionVerb.CHECK ||
         defaultActionVerb === DefaultActionVerb.OPEN ||
         defaultActionVerb === DefaultActionVerb.PRESS ||
         defaultActionVerb === DefaultActionVerb.SELECT ||
         defaultActionVerb === DefaultActionVerb.UNCHECK)) {
      cache.isActionable.set(node!, true);
      return true;
    }

    if (node!.role === RoleType.LIST_ITEM &&
        defaultActionVerb === DefaultActionVerb.CLICK) {
      cache.isActionable.set(node!, true);
      return true;
    }

    // Focusable items should be surfaced as either groups or actionable. So
    // should menu items.
    // Current heuristic is to show as actionble any focusable item where no
    // child is an interesting subtree.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (node!.state![StateType.FOCUSABLE] ||
        node!.role === RoleType.MENU_ITEM) {
      const result = !node!.children.some(
          (child: AutomationNode) =>
              SwitchAccessPredicate.isInterestingSubtree(child, cache));
      cache.isActionable.set(node!, result);
      return result;
    }
    return false;
  }

  /**
   * Returns true if |node| is a group, meaning that the node has more than one
   * interesting descendant, and that its interesting descendants exist in more
   * than one subtree of its immediate children.
   *
   * Additionally, for |node| to be a group, it cannot have the same bounding
   * box as its scope.
   */
  export function isGroup (
      node: AutomationNode | undefined,
      scope: AutomationNode | SARootNode | null,
      cache: SACache): boolean {
    // If node is invalid (undefined or an undefined role), return false.
    if (!node || !node.role) {
      return false;
    }
    if (cache.isGroup.has(node)) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      return cache.isGroup.get(node)!;
    }

    const scopeEqualsNode = scope &&
        (scope instanceof SARootNode ? scope.isEquivalentTo(node) :
                                       scope === node);
    if (scope && !scopeEqualsNode &&
        RectUtil.equal(node.location, scope.location)) {
      cache.isGroup.set(node, false);
      return false;
    }
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (node.state![StateType.INVISIBLE]) {
      cache.isGroup.set(node, false);
      return false;
    }

    if (node.role === RoleType.KEYBOARD) {
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
  }

  /**
   * Returns true if |node| is interesting for the user, meaning that |node|
   * is either actionable or a group.
   */
  export function isInteresting(
      node: AutomationNode | undefined, scope: AutomationNode | SARootNode,
      cache?: SACache): boolean {
    cache = cache || new SACache();
    return SwitchAccessPredicate.isActionable(node, cache) ||
        SwitchAccessPredicate.isGroup(node, scope, cache);
  }

  /** Returns true if the element is visible to the user for any reason. */
  export function isVisible(node: AutomationNode): boolean {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    return Boolean(
      !node.state![StateType.OFFSCREEN] && node.location &&
      node.location.top >= 0 && node.location.left >= 0 &&
      !node.state![StateType.INVISIBLE]);
  }

  /**
   * Returns true if there is an interesting node in the subtree containing
   * |node| as its root (including |node| itself).
   *
   * This function does not call isInteresting directly, because that would
   * cause a loop (isInteresting calls isGroup, and isGroup calls
   * isInterestingSubtree).
   */
  export function isInterestingSubtree(
      node: AutomationNode, cache?: SACache): boolean {
    cache = cache || new SACache();
    if (cache.isInterestingSubtree.has(node)) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      return cache.isInterestingSubtree.get(node)!;
    }
    const result = SwitchAccessPredicate.isActionable(node, cache) ||
        node.children.some(
            child => SwitchAccessPredicate.isInterestingSubtree(child, cache));
    cache.isInterestingSubtree.set(node, result);
    return result;
  }

  /** Returns true if |node| is an element that contains editable text. */
  export function isTextInput(node?: AutomationNode): boolean {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    return Boolean(node && node.state![StateType.EDITABLE]);
  }

  /** Returns true if |node| should be considered a window. */
  export function isWindow(node?: AutomationNode): boolean {
    return Boolean(
      node &&
      (node.role === RoleType.WINDOW ||
       (node.role === RoleType.CLIENT && node.parent &&
       node.parent.role === RoleType.WINDOW)));
  }

  /**
   * Returns a Restrictions object ready to be passed to AutomationTreeWalker.
   */
  export function restrictions(
      scope: AutomationNode): AutomationTreeWalkerRestriction {
    const cache = new SACache();
    return {
      leaf: SwitchAccessPredicate.leaf(scope, cache),
      root: SwitchAccessPredicate.root(scope),
      visit: SwitchAccessPredicate.visit(scope, cache),
    };
  }

  /**
   * Creates a function that confirms if |node| is a terminal leaf node of a
   * SwitchAccess scope tree when |scope| is the root.
   */
  export function leaf(
      scope: AutomationNode, cache: SACache): AutomationPredicate.Unary {
    return (node: AutomationNode) =>
        !SwitchAccessPredicate.isInterestingSubtree(node, cache) ||
        (scope !== node &&
         SwitchAccessPredicate.isInteresting(node, scope, cache));
  }

  /**
   * Creates a function that confirms if |node| is the root of a SwitchAccess
   * scope tree when |scope| is the root.
   */
  export function root(scope: AutomationNode): AutomationPredicate.Unary {
    return (node: AutomationNode) => scope === node;
  }

  /**
   * Creates a function that determines whether |node| is to be visited in the
   * SwitchAccess scope tree with |scope| as the root.
   */
  export function visit(
      scope: AutomationNode, cache: SACache): AutomationPredicate.Unary {
    return (node: AutomationNode) => node.role !== RoleType.DESKTOP &&
        SwitchAccessPredicate.isInteresting(node, scope, cache);
  }
}

TestImportManager.exportForTesting(
  ['SwitchAccessPredicate', SwitchAccessPredicate]);
