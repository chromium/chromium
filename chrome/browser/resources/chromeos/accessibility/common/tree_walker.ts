// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A tree walker over the automation tree.
 */

import {AutomationPredicate} from './automation_predicate.js';
import {constants} from './constants.js';
import {TestImportManager} from './testing/test_import_manager.js';

type AutomationNode = chrome.automation.AutomationNode;

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

export interface AutomationTreeWalkerRestriction {
  leaf?: AutomationPredicate.Unary;
  root?: AutomationPredicate.Unary;
  visit?: AutomationPredicate.Unary;
  skipInitialAncestry?: boolean;
  skipInitialSubtree?: boolean;
}

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
  // TODO(b/314204374): Convert from null to undefined.
  private node_: AutomationNode|null;
  private phase_: string;
  private dir_: constants.Dir;
  private initialNode_: AutomationNode;
  // TODO(b/314204374): Convert from null to undefined.
  private backwardAncestor_: AutomationNode|null;
  private visitPred_: AutomationPredicate.Unary;
  private skipInitialAncestry_: boolean;
  private skipInitialSubtree_: boolean;
  private leafPred_: any;
  private rootPred_: any;

  constructor(
      node: AutomationNode, dir: constants.Dir,
      restrictions: AutomationTreeWalkerRestriction = {}) {
    this.node_ = node;
    this.phase_ = AutomationTreeWalkerPhase.INITIAL;
    this.dir_ = dir;
    this.initialNode_ = node;

    /**
     * Deepest common ancestor of initialNode and node. Valid only when moving
     * backward.
     */
    this.backwardAncestor_ = node.parent ?? null;

    this.visitPred_ = this.makeVisitPred_(restrictions);
    this.leafPred_ = restrictions.leaf ?? falsePredicate;
    this.rootPred_ = restrictions.root ?? falsePredicate;
    this.skipInitialAncestry_ = restrictions.skipInitialAncestry ?? false;
    this.skipInitialSubtree_ = restrictions.skipInitialSubtree ?? false;
  }

  private makeVisitPred_(restrictions: AutomationTreeWalkerRestriction):
      AutomationPredicate.Unary {
    return node => {
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
  }

  get node(): AutomationNode|null {
    return this.node_;
  }

  get phase(): string {
    return this.phase_;
  }

  /**
   * Moves this walker to the next node.
   * @return The called AutomationTreeWalker, for chaining.
   */
  next(): AutomationTreeWalker {
    if (!this.node_) {
      return this;
    }

    do {
      if (this.rootPred_(this.node_) && this.dir_ === constants.Dir.BACKWARD) {
        this.node_ = null;
        return this;
      }
      if (this.dir_ === constants.Dir.FORWARD) {
        this.forward_(this.node_);
      } else {
        this.backward_(this.node_);
      }
    } while (this.node_ && !this.visitPred_(this.node_));
    return this;
  }

  private forward_(node: AutomationNode): void {
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

    let searchNode: AutomationNode|undefined = node;
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

  private backward_(node: AutomationNode): void {
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

// Local to module.

function falsePredicate(_node: AutomationNode): boolean {
  return false;
}

TestImportManager.exportForTesting(AutomationTreeWalker);
