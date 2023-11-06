// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A tree walker over the automation tree.
 */

import {AutomationPredicate} from './automation_predicate.js';
import {constants} from './constants.js';

const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;

/**
 * Defined phases of traversal from the initial node passed to an
 * AutomationTreeWalker instance.
 * @enum {string}
 */
export const AutomationTreeWalkerPhase = {
  // Walker is on the initial node.
  INITIAL: 'initial',
  // Walker is on an ancestor of initial node.
  ANCESTOR: 'ancestor',
  // Walker is on a descendant of initial node.
  DESCENDANT: 'descendant',
  // Walker is on a node not covered by any other phase.
  OTHER: 'other',
};

/**
 * @typedef {{leaf: (AutomationPredicate.Unary|undefined),
 *            root: (AutomationPredicate.Unary|undefined),
 *            visit: (AutomationPredicate.Unary|undefined),
 *            skipInitialAncestry: (boolean|undefined),
 *            skipInitialSubtree: (boolean|undefined)}}
 */
export let AutomationTreeWalkerRestriction;

/**
 * An AutomationTreeWalker provides an incremental pre order traversal of the
 * automation tree starting at a particular node.
 * Given a flat list of nodes in pre order, the walker moves forward or backward
 * a node at a time on each call of |next|.
 * A caller can visit a subset of this list by supplying restricting
 * predicates. There are three such restricting predicates allowed:
 * visit: this predicate determines if a given node should be returned when
 * moving to a node in the flattened pre-order list. If not, this walker will
 * continue to the next (directed) node in the list, looking for a predicate
 * match.
 *   root: this predicate determines if a node should end upward movement in
 *     the tree.
 *   leaf: this predicate determines if a node should end downward movement in
 *     the tree.
 *   skipInitialAncestry: skips visiting ancestor nodes of the start node for
 *     multiple invocations of next when moving backward.
 *   skipInitialSubtree: makes the first invocation of |next| skip the initial
 *     node's subtree when finding a match. This is useful to establish a known
 *     initial state when the initial node may not match any of the given
 *     predicates.
 * Given the above definitions, if supplied with a root and leaf predicate that
 * always returns false, and a visit predicate that always returns true, the
 * walker would visit all nodes in pre order. If a caller does not supply a
 * particular predicate, it will default to these "identity" predicates.
 */
export class AutomationTreeWalker {
  /**
   * @param {!AutomationNode} node
   * @param {!Dir} dir
   * @param {AutomationTreeWalkerRestriction=}
   *        opt_restrictions
   */
  constructor(node, dir, opt_restrictions) {
    /** @private {?AutomationNode} */
    this.node_ = node;
    /** @private {!AutomationTreeWalkerPhase} */
    this.phase_ = AutomationTreeWalkerPhase.INITIAL;
    /** @private {!Dir} */
    this.dir_ = dir;
    /** @private {!AutomationNode} */
    this.initialNode_ = node;
    /**
     * Deepest common ancestor of initialNode and node. Valid only when moving
     * backward.
     * @private {?AutomationNode}
     */
    this.backwardAncestor_ = node.parent || null;
    const restrictions = opt_restrictions || {};

    this.visitPred_ = function(node) {
      if (this.skipInitialAncestry_ &&
          this.phase_ === AutomationTreeWalkerPhase.ANCESTOR) {
        return false;
      }

      if (this.skipInitialSubtree_ &&
          this.phase_ !== AutomationTreeWalkerPhase.ANCESTOR &&
          this.phase_ !== AutomationTreeWalkerPhase.OTHER) {
        return false;
      }

      if (restrictions.visit) {
        return restrictions.visit(node);
      }

      return true;
    };
    /** @private {AutomationPredicate.Unary} */
    this.leafPred_ = restrictions.leaf ? restrictions.leaf :
                                         AutomationTreeWalker.falsePredicate_;
    /** @private {AutomationPredicate.Unary} */
    this.rootPred_ = restrictions.root ? restrictions.root :
                                         AutomationTreeWalker.falsePredicate_;
    /** @private {boolean} */
    this.skipInitialAncestry_ = restrictions.skipInitialAncestry || false;
    /** @private {boolean} */
    this.skipInitialSubtree_ = restrictions.skipInitialSubtree || false;
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   * @private
   */
  static falsePredicate_(node) {
    return false;
  }

  /** @type {AutomationNode} */
  get node() {
    return this.node_;
  }

  /** @type {AutomationTreeWalkerPhase} */
  get phase() {
    return this.phase_;
  }

  /**
   * Moves this walker to the next node.
   * @return {!AutomationTreeWalker} The called AutomationTreeWalker, for
   *                                 chaining.
   */
  next() {
    if (!this.node_) {
      return this;
    }

    do {
      if (this.rootPred_(this.node_) && this.dir_ === Dir.BACKWARD) {
        this.node_ = null;
        return this;
      }
      if (this.dir_ === Dir.FORWARD) {
        this.forward_(this.node_);
      } else {
        this.backward_(this.node_);
      }
    } while (this.node_ && !this.visitPred_(this.node_));
    return this;
  }

  /**
   * @param {!AutomationNode} node
   * @private
   */
  forward_(node) {
    if (!this.leafPred_(node) && node.firstChild) {
      if (this.phase_ === AutomationTreeWalkerPhase.INITIAL) {
        this.phase_ = AutomationTreeWalkerPhase.DESCENDANT;
      }

      if (!this.skipInitialSubtree_ ||
          this.phase_ !== AutomationTreeWalkerPhase.DESCENDANT) {
        this.node_ = node.firstChild;
        return;
      }
    }

    let searchNode = node;
    while (searchNode) {
      // We have crossed out of the initial node's subtree for either a
      // sibling or parent move.
      if (searchNode === this.initialNode_) {
        this.phase_ = AutomationTreeWalkerPhase.OTHER;
      }

      if (searchNode.nextSibling) {
        this.node_ = searchNode.nextSibling;
        return;
      }

      // Update the phase based on the parent if needed since we may exit below.
      if (searchNode.parent === this.initialNode_) {
        this.phase_ = AutomationTreeWalkerPhase.OTHER;
      }

      // Exit if we encounter a root-like node and are not searching descendants
      // of the initial node.
      if (searchNode.parent && this.rootPred_(searchNode.parent) &&
          this.phase_ !== AutomationTreeWalkerPhase.DESCENDANT) {
        break;
      }

      searchNode = searchNode.parent;
    }
    this.node_ = null;
  }

  /**
   * @param {!AutomationNode} node
   * @private
   */
  backward_(node) {
    if (node.previousSibling) {
      this.phase_ = AutomationTreeWalkerPhase.OTHER;
      node = node.previousSibling;

      while (!this.leafPred_(node) && node.lastChild) {
        node = node.lastChild;
      }

      this.node_ = node;
      return;
    }
    if (node.parent && this.backwardAncestor_ === node.parent) {
      this.phase_ = AutomationTreeWalkerPhase.ANCESTOR;
      this.backwardAncestor_ = node.parent.parent || null;
    }
    this.node_ = node.parent || null;
  }
}
