// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utilities for the automation extension API.
 */

import {AutomationPredicate} from './automation_predicate.js';
import {constants} from './constants.js';
import {TestImportManager} from './testing/test_import_manager.js';
import {AutomationTreeWalker, AutomationTreeWalkerRestriction} from './tree_walker.js';

type AutomationNode = chrome.automation.AutomationNode;
const HasPopup = chrome.automation.HasPopup;
const RoleType = chrome.automation.RoleType;

export class AutomationUtil {
  /**
   * Find a node in subtree of |cur| satisfying |pred| using pre-order
   * traversal.
   * @param cur Node to begin the search from.
   * @param pred A predicate to apply to a candidate node.
   * @return the node found, or null if none was found.
   */
  static findNodePre(
      cur: AutomationNode, dir: constants.Dir,
      pred: AutomationPredicate.Unary): AutomationNode|null {
    if (!cur) {
      return null;
    }

    if (pred(cur) && !AutomationPredicate.shouldIgnoreNode(cur)) {
      return cur;
    }

    let child = dir === constants.Dir.BACKWARD ? cur.lastChild : cur.firstChild;
    while (child) {
      const ret = AutomationUtil.findNodePre(child, dir, pred);
      if (ret) {
        return ret;
      }
      child = dir === constants.Dir.BACKWARD ? child.previousSibling :
                                               child.nextSibling;
    }
    return null;
  }

  /**
   * For a given automation property, return true if the value
   * represents something 'truthy', e.g.: for checked:
   * 'true'|'mixed' -> true
   * 'false'|undefined -> false
   * @return True if the value represents something 'truthy'.
   */
  static isTruthy(node: AutomationNode, attrib: string): boolean {
    if (!node) {
      return false;
    }
    switch (attrib) {
      case 'checked':
        return Boolean(node.checked) && node.checked !== 'false';
      case 'hasPopup':
        return Boolean(node.hasPopup) && node.hasPopup !== HasPopup.FALSE;

      // Chrome automatically calculates these attributes.
      case 'posInSet':
        return node.root!.role !== RoleType.ROOT_WEB_AREA &&
            Boolean(node.posInSet);
      case 'setSize':
        return Boolean(node.setSize);

      // These attributes default to false for empty strings.
      case 'roleDescription':
        return Boolean(node.roleDescription);
      case 'value':
        return Boolean(node.value);
      case 'selected':
        return node.selected === true;
      default:
        // @ts-ignore: TODO(b/267329383): expression of type 'string' can't be
        // used to index type 'AutomationNode'.
        return node[attrib] !== undefined || Boolean(node.state[(attrib)]);
    }
  }

  /**
   * For a given automation property, return true if the value
   * represents something 'falsey', e.g.: for selected:
   * node.selected === false
   * @return If it represents something 'falsey'.
   */
  static isFalsey(node: AutomationNode, attrib: string): boolean {
    if (!node) {
      return false;
    }
    switch (attrib) {
      case 'selected':
        return node.selected === false;
      default:
        return !AutomationUtil.isTruthy(node, attrib);
    }
  }


  /**
   * Find a node in subtree of |cur| satisfying |pred| using post-order
   * traversal.
   * @param cur Node to begin the search from.
   * @param pred A predicate to apply
   *     to a candidate node.
   * @return The node found or null
   */
  static findNodePost(
      cur: AutomationNode, dir: constants.Dir,
      pred: AutomationPredicate.Unary): AutomationNode|null {
    if (!cur) {
      return null;
    }

    let child = dir === constants.Dir.BACKWARD ? cur.lastChild : cur.firstChild;
    while (child) {
      const ret = AutomationUtil.findNodePost(child, dir, pred);
      if (ret) {
        return ret;
      }
      child = dir === constants.Dir.BACKWARD ? child.previousSibling :
                                               child.nextSibling;
    }

    if (pred(cur) && !AutomationPredicate.shouldIgnoreNode(cur)) {
      return cur;
    }

    return null;
  }

  /**
   * Find the next node in the given direction in depth first order.
   *
   * Let D be the dfs linearization of |cur.root|. Then, let F be the list after
   * applying |pred| as a filter to D. This method will return the directed next
   * node of |cur| in F.
   * The restrictions option will further filter F. For example,
   * |skipInitialSubtree| will remove any |pred| matches in the subtree of |cur|
   * from F.
   * @param cur Node to begin the search from.
   * @param pred A predicate to apply to a candidate node.
   * @param optRestrictions |leaf|, |root|,
   *     |skipInitialAncestry|, and |skipInitialSubtree| are valid restrictions
   *     used when finding the next node.
   *     By default:
   *        the root predicate gets set to |AutomationPredicate.root|.
   *        |skipInitialSubtree| is false if |cur| is a container or matches
   *        |pred|. This alleviates the caller from syncing forwards.
   *        Leaves are nodes matched by |pred| which are not also containers.
   *        This takes care of syncing backwards.
   * @return The next node found
   */
  static findNextNode(
      cur: AutomationNode, dir: constants.Dir, pred: AutomationPredicate.Unary,
      optRestrictions?: AutomationTreeWalkerRestriction): AutomationNode|null {
    const walker = createWalker(cur, dir, pred, optRestrictions);
    return walker.next().node;
  }

  /**
   * Finds all nodes in the given direction in depth first order.
   *
   * Let D be the dfs linearization of |cur.root|. Then, let F be the list after
   * applying |pred| as a filter to D. This method will return the directed next
   * node of |cur| in F.
   * The restrictions option will further filter F. For example,
   * |skipInitialSubtree| will remove any |pred| matches in the subtree of |cur|
   * from F.
   * @param cur Node to begin the search from.
   * @param pred A predicate to apply to a candidate node.
   * @param optRestrictions |leaf|, |root|,
   *     |skipInitialAncestry|, and |skipInitialSubtree| are valid restrictions
   *     used when finding the next node.
   *     By default:
   *        the root predicate gets set to |AutomationPredicate.root|.
   *        |skipInitialSubtree| is false if |cur| is a container or matches
   *        |pred|. This alleviates the caller from syncing forwards.
   *        Leaves are nodes matched by |pred| which are not also containers.
   *        This takes care of syncing backwards.
   * @return All the nodes found.
   */
  static findAllNodes(
      cur: AutomationNode, dir: constants.Dir, pred: AutomationPredicate.Unary,
      optRestrictions?: AutomationTreeWalkerRestriction): AutomationNode[] {
    const walker = createWalker(cur, dir, pred, optRestrictions);
    const nodes = [];
    let currentNode = walker.next().node;
    while (currentNode) {
      nodes.push(currentNode);
      currentNode = walker.next().node;
    }
    return nodes;
  }

  /**
   * Given nodes a_1, ..., a_n starting at |cur| in pre order traversal, apply
   * |pred| to a_i and a_(i - 1) until |pred| is satisfied.  Returns a_(i - 1)
   * or a_i (depending on optBefore) or null if no match was found.
   * @param optBefore True to return a_(i - 1); a_i otherwise.
   *                  Defaults to false.
   * @return The node found.
   */
  static findNodeUntil(
      cur: AutomationNode, dir: constants.Dir, pred: AutomationPredicate.Binary,
      optBefore?: boolean): AutomationNode|null {
    let before = cur;
    let after: AutomationNode|null = before;
    do {
      before = after;
      after =
          AutomationUtil.findNextNode(before, dir, AutomationPredicate.leaf);
    } while (after && !pred(before, after));
    return optBefore ? before : after;
  }

  /**
   * Returns an array containing ancestors of node starting at root down to
   * node.
   * @return The array of ancestors found.
   */
  static getAncestors(node: AutomationNode): AutomationNode[] {
    const ret = [];
    let candidate: AutomationNode|undefined = node;
    while (candidate) {
      ret.push(candidate);

      candidate = candidate.parent;
    }
    return ret.reverse();
  }

  /**
   * Finds the lowest ancestor with a given role.
   * @return The ancestor found.
   */
  static getFirstAncestorWithRole(
      node: AutomationNode, role: chrome.automation.RoleType): AutomationNode
      |null {
    if (!node.parent) {
      return null;
    }
    if (node.parent.role === role) {
      return node.parent;
    }
    return AutomationUtil.getFirstAncestorWithRole(node.parent, role);
  }

  /**
   * Gets the first index where the two input arrays differ. Returns -1 if they
   * do not.
   * @return The index or -1.
   */
  static getDivergence(
      ancestorsA: AutomationNode[], ancestorsB: AutomationNode[]): number {
    for (let i = 0; i < ancestorsA.length; i++) {
      if (ancestorsA[i] !== ancestorsB[i]) {
        return i;
      }
    }
    if (ancestorsA.length === ancestorsB.length) {
      return -1;
    }
    return ancestorsA.length;
  }

  /**
   * Returns ancestors of |node| that are not also ancestors of |prevNode|.
   * @return The ancestors found.
   */
  static getUniqueAncestors(prevNode: AutomationNode, node: AutomationNode):
      AutomationNode[] {
    const prevAncestors = AutomationUtil.getAncestors(prevNode);
    const ancestors = AutomationUtil.getAncestors(node);
    const divergence = AutomationUtil.getDivergence(prevAncestors, ancestors);
    return ancestors.slice(divergence);
  }

  /**
   * Given |nodeA| and |nodeB| in that order, determines their ordering in the
   * document.
   * @return The direction representing the ordering.
   */
  static getDirection(nodeA: AutomationNode, nodeB: AutomationNode):
      constants.Dir {
    const ancestorsA = AutomationUtil.getAncestors(nodeA);
    const ancestorsB = AutomationUtil.getAncestors(nodeB);
    const divergence = AutomationUtil.getDivergence(ancestorsA, ancestorsB);

    // Default to constants.Dir.FORWARD.
    if (divergence === -1) {
      return constants.Dir.FORWARD;
    }

    const divA = ancestorsA[divergence];
    const divB = ancestorsB[divergence];

    // One of the nodes is an ancestor of the other. Order this relationship in
    // the same way dfs would. nodeA <= nodeB if nodeA is a descendant of
    // nodeB. nodeA > nodeB if nodeB is a descendant of nodeA.

    if (!divA) {
      return constants.Dir.FORWARD;
    }
    if (!divB) {
      return constants.Dir.BACKWARD;
    }
    if (divA.parent === nodeB) {
      return constants.Dir.BACKWARD;
    }
    if (divB.parent === nodeA) {
      return constants.Dir.FORWARD;
    }

    // TODO(b/267329383): indexInParent may be undefined.
    return divA.indexInParent! <= divB.indexInParent!? constants.Dir.FORWARD :
                                                       constants.Dir.BACKWARD;
  }

  /**
   * Determines whether the two given nodes come from the same tree source.
   */
  static isInSameTree(a: AutomationNode, b: AutomationNode): boolean {
    if (!a || !b) {
      return true;
    }

    // Given two non-desktop roots, consider them in the "same" tree.
    return a.root === b.root ||
        // TODO(b/267329383): a.root and b.root may be undefined.
        (a.root!.role === b.root!.role &&
         a.root!.role === RoleType.ROOT_WEB_AREA);
  }

  /**
   * Determines whether or not a node is or is the descendant of another node.
   * @return Whether the node is a descendant of the other node.
   */
  static isDescendantOf(node: AutomationNode, ancestor: AutomationNode):
      boolean {
    let testNode: AutomationNode|undefined = node;
    while (testNode && testNode !== ancestor) {
      testNode = testNode.parent;
    }
    return testNode === ancestor;
  }

  /**
   * Finds the deepest node containing the point. Since the automation tree does
   * not maintain a containment invariant when considering child node bounding
   * rects with respect to their parents, the hit test considers all children
   * before their parents when looking for a matching node.
   * @param node Subtree to search.
   * @return The deepest node containing the point.
   */
  static hitTest(node: AutomationNode, point: constants.Point): AutomationNode
      |null {
    let child = node.firstChild;
    while (child) {
      const hit = AutomationUtil.hitTest(child, point);
      if (hit) {
        return hit;
      }
      child = child.nextSibling;
    }

    const loc = node.unclippedLocation;

    // When |node| is partially or fully offscreen, try to find a better match.
    // TODO(b/267329383): loc may be undefined.
    if (loc!.left < 0 || loc!.top < 0) {
      return null;
    }

    if (point.x <= (loc!.left + loc!.width) && point.x >= loc!.left &&
        point.y <= (loc!.top + loc!.height) && point.y >= loc!.top) {
      return node;
    }
    return null;
  }

  /**
   * Gets a top level root.
   * @return The top level root.
   */
  static getTopLevelRoot(node: AutomationNode): AutomationNode|null {
    let root = node.root;
    if (!root || root.role === RoleType.DESKTOP) {
      return null;
    }

    while (root && root.parent && root.parent.root &&
           root.parent.root.role !== RoleType.DESKTOP) {
      root = root.parent.root;
    }
    return root;
  }

  /**
   * @return The least common ancestor of the two nodes.
   */
  static getLeastCommonAncestor(prevNode: AutomationNode, node: AutomationNode):
      AutomationNode|undefined {
    if (prevNode === node) {
      return node;
    }

    const prevAncestors = AutomationUtil.getAncestors(prevNode);
    const ancestors = AutomationUtil.getAncestors(node);
    const divergence = AutomationUtil.getDivergence(prevAncestors, ancestors);
    return ancestors[divergence - 1]!;
  }

  /**
   * Gets the accessible text for this node based on its role.
   * This text is suitable for caret navigation and selection in the node.
   * @return The accessible text.
   */
  static getText(node: AutomationNode): string {
    if (!node) {
      return '';
    }

    if (node.role === RoleType.TEXT_FIELD) {
      return node.value || '';
    }
    return node.name || '';
  }

  /**
   * Gets the root of editable node.
   * @return The root if it is editable and focused.
   */
  static getEditableRoot(node: AutomationNode): AutomationNode|undefined {
    let testNode: AutomationNode|undefined = node;
    let rootEditable;
    do {
      // TODO(b/267329383): testNode.state may be undefined.
      if (testNode.state!['editable'] && testNode.state!['focused']) {
        rootEditable = testNode;
      }
      testNode = testNode.parent;
    } while (testNode);
    return rootEditable;
  }

  /**
   * Gets the last (DFS) ordered node matched by a predicate assuming a
   * preference for ancestors.
   *
   * In detail:
   * Given a DFS ordering on nodes a_1, ..., a_n, applying a predicate
   * from 1 to n yields a different set of nodes from that when applying
   * a predicate from n to 1 if we skip the remaining descendants of a
   * successfully matched node when moving forward. To recover the same
   * nodes when applying the predicate from n to 1, we make the
   * observation that we want the shallowest node that matches the
   * predicate in a successfully matched node's ancestry chain.
   * Note that container nodes should only be considered if there are no current
   * matches.
   * @param root Tree to search.
   * @param pred A predicate to apply
   * @return The node found.
   */
  static findLastNode(root: AutomationNode, pred: AutomationPredicate.Unary):
      AutomationNode|null {
    let node: AutomationNode|null = root;
    while (node.lastChild) {
      node = node.lastChild;
    }

    do {
      if (AutomationPredicate.shouldIgnoreNode(node)) {
        continue;
      }

      // Get the shallowest node matching the predicate.
      let walker: AutomationNode|undefined = node;
      let shallowest = null;
      while (walker) {
        if (walker === root) {
          break;
        }

        if (pred(walker) && !AutomationPredicate.shouldIgnoreNode(walker) &&
            (!shallowest || !AutomationPredicate.container(walker))) {
          shallowest = walker;
        }

        walker = walker.parent;
      }

      if (shallowest) {
        return shallowest;
      }
    } while (
        node = AutomationUtil.findNextNode(node, constants.Dir.BACKWARD, pred));

    return null;
  }
}

/**
 * @param cur Node to begin the search from.
 * @param pred A predicate to apply to a candidate node.
 * @param optRestrictions |leaf|, |root|,
 *     |skipInitialAncestry|, and |skipInitialSubtree| are valid restrictions
 *     used when finding the next node.
 * @return Instance of tree walker initialized with given parameters.
 */
function createWalker(
    cur: AutomationNode, dir: constants.Dir, pred: AutomationPredicate.Unary,
    optRestrictions?: AutomationTreeWalkerRestriction): AutomationTreeWalker {
  const restrictions: AutomationTreeWalkerRestriction = {};
  optRestrictions = optRestrictions || {
    skipInitialSubtree: !AutomationPredicate.container(cur) && pred(cur),
  };

  restrictions.root = optRestrictions.root || AutomationPredicate.root;
  restrictions.leaf = optRestrictions.leaf || function(node) {
    // Treat nodes matched by |pred| as leaves except for containers.
    return !AutomationPredicate.container(node) && pred(node);
  };

  restrictions.skipInitialSubtree = optRestrictions.skipInitialSubtree;
  restrictions.skipInitialAncestry = optRestrictions.skipInitialAncestry;

  restrictions.visit = function(node: AutomationNode) {
    return pred(node) && !AutomationPredicate.shouldIgnoreNode(node);
  };

  return new AutomationTreeWalker(cur, dir, restrictions);
}

TestImportManager.exportForTesting(AutomationUtil);
