// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines various strategies for recovering automation nodes.
 */

goog.provide('AncestryRecoveryStrategy');
goog.provide('RecoveryStrategy');
goog.provide('TreePathRecoveryStrategy');

goog.scope(function() {
var AutomationNode = chrome.automation.AutomationNode;
var RoleType = chrome.automation.RoleType;

/**
 * @param {!AutomationNode} node
 * @constructor
 */
RecoveryStrategy = function(node) {
  /** @private {!AutomationNode} */
  this.node_ = node;
};

RecoveryStrategy.prototype = {
  /** @return {!AutomationNode} */
  get node() {
    if (this.requiresRecovery()) {
      this.node_ = this.recover() || this.node_;
    }

    return this.node_;
  },

  /** @return {boolean} */
  requiresRecovery: function() {
    return !this.node_ || !this.node_.role;
  },

  /**
   * @return {AutomationNode}
   * @protected
   */
  recover: function() {
    return null;
  }
};

/**
 * A recovery strategy that uses the node's ancestors.
 * @constructor
 * @extends {RecoveryStrategy}
 */
AncestryRecoveryStrategy = function(node) {
  RecoveryStrategy.call(this, node);

  /** @type {!Array<AutomationNode>} @private */
  this.ancestry_ = [];
  var nodeWalker = node;
  while (nodeWalker) {
    this.ancestry_.push(nodeWalker);
    nodeWalker = nodeWalker.parent;
    if (nodeWalker && nodeWalker.role == RoleType.WINDOW) {
      break;
    }
  }
};

AncestryRecoveryStrategy.prototype = {
  __proto__: RecoveryStrategy.prototype,

  /** @override */
  recover: function() {
    return this.ancestry_[this.getFirstValidNodeIndex_()];
  },

  /**
   * @return {number}
   * @protected
   */
  getFirstValidNodeIndex_: function() {
    for (var i = 0; i < this.ancestry_.length; i++) {
      var firstValidNode = this.ancestry_[i];
      if (firstValidNode != null && firstValidNode.role !== undefined &&
          firstValidNode.root != undefined) {
        return i;
      }
    }
    return 0;
  }
};

/**
 * A recovery strategy that uses the node's tree path.
 * @constructor
 * @extends {AncestryRecoveryStrategy}
 */
TreePathRecoveryStrategy = function(node) {
  AncestryRecoveryStrategy.call(this, node);

  /** @type {!Array<number>} @private */
  this.recoveryChildIndex_ = [];
  var nodeWalker = node;
  while (nodeWalker) {
    this.recoveryChildIndex_.push(nodeWalker.indexInParent);
    nodeWalker = nodeWalker.parent;
    if (nodeWalker && nodeWalker.role == RoleType.WINDOW) {
      break;
    }
  }
};

TreePathRecoveryStrategy.prototype = {
  __proto__: AncestryRecoveryStrategy.prototype,

  /** @override */
  recover: function() {
    var index = this.getFirstValidNodeIndex_();
    if (index == 0) {
      return this.ancestry_[index];
    }

    // Otherwise, attempt to recover.
    var node = this.ancestry_[index];
    for (var j = index - 1; j >= 0; j--) {
      var childIndex = this.recoveryChildIndex_[j];
      var children = node.children;
      if (!children[childIndex]) {
        return node;
      }
      node = children[childIndex];
    }
    return node;
  }
};
});  // goog.scope
