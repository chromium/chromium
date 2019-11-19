// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A tree walker over the automation tree.
 */

goog.provide('AutomationTreeWalker');
goog.provide('AutomationTreeWalkerPhase');
goog.provide('AutomationTreeWalkerRestriction');

goog.require('constants');

/**
 * Defined phases of traversal from the initial node passed to an
 * AutomationTreeWalker instance.
 * @enum {string}
 */
AutomationTreeWalkerPhase = {
  /** Walker is on the initial node. */
  INITIAL: 'initial',
  /** Walker is on an ancestor of initial node. */
  ANCESTOR: 'ancestor',
  /** Walker is on a descendant of initial node. */
  DESCENDANT: 'descendant',
  /** Walker is on a node not covered by any other phase. */
  OTHER: 'other'
};

/**
 * @typedef {{leaf: (AutomationPredicate.Unary|undefined),
 *          root: (AutomationPredicate.Unary|undefined),
 *          visit: (AutomationPredicate.Unary|undefined),
 *          skipInitialAncestry: (boolean|undefined),
 *          skipInitialSubtree: (boolean|undefined)}}
 */
var AutomationTreeWalkerRestriction;

/**
 * An AutomationTreeWalker provides an incremental pre order traversal of the
 * automation tree starting at a particular node.
 *
 * Given a flat list of nodes in pre order, the walker moves forward or backward
 * a node at a time on each call of |next|.
 *
 * A caller can visit a subset of this list by supplying restricting
 * predicates. There are three such restricting predicates allowed:
 * visit: this predicate determines if a given node should be returned when
 * moving to a node in the flattened pre-order list. If not, this walker will
 * continue to the next (directed) node in the list, looking for a predicate
 * match.
 * root: this predicate determines if a node should end upward movement in the
 * tree.
 * leaf: this predicate determines if a node should end downward movement in the
 * tree.
 *
 * |skipInitialAncestry| skips visiting ancestor nodes of the start node for
 * multiple invokations of next when moving backward.
 *
 * Finally, a boolean, |skipInitialSubtree|, makes the first invocation of
 * |next| skip the initial node's subtree when finding a match. This is useful
 * to establish a known initial state when the initial node may not match any of
 * the given predicates.
 *
 * Given the above definitions, if supplied with a root and leaf predicate that
 * always returns false, and a visit predicate that always returns true, the
 * walker would visit all nodes in pre order. If a caller does not supply a
 * particular predicate, it will default to these "identity" predicates.
 *
 * @param {!chrome.automation.AutomationNode} node
 * @param {constants.Dir} dir
 * @param {AutomationTreeWalkerRestriction=}
 *        opt_restrictions
 * @constructor
 */
AutomationTreeWalker = function(node, dir, opt_restrictions) {
  /** @type {chrome.automation.AutomationNode} @private */
  this.node_ = node;
  /** @type {AutomationTreeWalkerPhase} @private */
  this.phase_ = AutomationTreeWalkerPhase.INITIAL;
  /** @const {constants.Dir} @private */
  this.dir_ = dir;
  /** @const {!chrome.automation.AutomationNode} @private */
  this.initialNode_ = node;
  /**
   * Deepest common ancestor of initialNode and node. Valid only when moving
   * backward.
   * @type {chrome.automation.AutomationNode} @private
   */
  this.backwardAncestor_ = node.parent || null;
  var restrictions = opt_restrictions || {};

  this.visitPred_ = function(node) {
    if (this.skipInitialAncestry_ &&
        this.phase_ == AutomationTreeWalkerPhase.ANCESTOR) {
      return false;
    }

    if (this.skipInitialSubtree_ &&
        this.phase_ != AutomationTreeWalkerPhase.ANCESTOR &&
        this.phase_ != AutomationTreeWalkerPhase.OTHER) {
      return false;
    }

    if (restrictions.visit) {
      return restrictions.visit(node);
    }

    return true;
  };
  /** @type {AutomationPredicate.Unary} @private */
  this.leafPred_ = restrictions.leaf ? restrictions.leaf :
                                       AutomationTreeWalker.falsePredicate_;
  /** @type {AutomationPredicate.Unary} @private */
  this.rootPred_ = restrictions.root ? restrictions.root :
                                       AutomationTreeWalker.falsePredicate_;
  /** @const {boolean} @private */
  this.skipInitialAncestry_ = restrictions.skipInitialAncestry || false;
  /** @const {boolean} @private */
  this.skipInitialSubtree_ = restrictions.skipInitialSubtree || false;
};

/**
 * @param {!chrome.automation.AutomationNode} node
 * @return {boolean}
 * @private
 */
AutomationTreeWalker.falsePredicate_ = function(node) {
  return false;
};

AutomationTreeWalker.prototype = {
  /** @type {chrome.automation.AutomationNode} */
  get node() {
    return this.node_;
  },

  /** @type {AutomationTreeWalkerPhase} */
  get phase() {
    return this.phase_;
  },

  /**
   * Moves this walker to the next node.
   * @return {!AutomationTreeWalker} The called AutomationTreeWalker, for
   *                                 chaining.
   */
  next: function() {
    if (!this.node_) {
      return this;
    }

    do {
      if (this.rootPred_(this.node_) && this.dir_ == constants.Dir.BACKWARD) {
        this.node_ = null;
        return this;
      }
      if (this.dir_ == constants.Dir.FORWARD) {
        this.forward_(this.node_);
      } else {
        this.backward_(this.node_);
      }
    } while (this.node_ && !this.visitPred_(this.node_));
    return this;
  },

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @private
   */
  forward_: function(node) {
    if (!this.leafPred_(node) && node.firstChild) {
      if (this.phase_ == AutomationTreeWalkerPhase.INITIAL) {
        this.phase_ = AutomationTreeWalkerPhase.DESCENDANT;
      }

      if (!this.skipInitialSubtree_ ||
          this.phase_ != AutomationTreeWalkerPhase.DESCENDANT) {
        this.node_ = node.firstChild;
        return;
      }
    }

    var searchNode = node;
    while (searchNode) {
      // We have crossed out of the initial node's subtree for either a
      // sibling or parent move.
      if (searchNode == this.initialNode_) {
        this.phase_ = AutomationTreeWalkerPhase.OTHER;
      }

      if (searchNode.nextSibling) {
        this.node_ = searchNode.nextSibling;
        return;
      }

      // Update the phase based on the parent if needed since we may exit below.
      if (searchNode.parent == this.initialNode_) {
        this.phase_ = AutomationTreeWalkerPhase.OTHER;
      }

      // Exit if we encounter a root-like node and are not searching descendants
      // of the initial node.
      if (searchNode.parent && this.rootPred_(searchNode.parent) &&
          this.phase_ != AutomationTreeWalkerPhase.DESCENDANT) {
        break;
      }

      searchNode = searchNode.parent;
    }
    this.node_ = null;
  },

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @private
   */
  backward_: function(node) {
    if (node.previousSibling) {
      this.phase_ = AutomationTreeWalkerPhase.OTHER;
      node = node.previousSibling;

      while (!this.leafPred_(node) && node.lastChild) {
        node = node.lastChild;
      }

      this.node_ = node;
      return;
    }
    if (node.parent && this.backwardAncestor_ == node.parent) {
      this.phase_ = AutomationTreeWalkerPhase.ANCESTOR;
      this.backwardAncestor_ = node.parent.parent || null;
    }
    this.node_ = node.parent || null;
  }
};
