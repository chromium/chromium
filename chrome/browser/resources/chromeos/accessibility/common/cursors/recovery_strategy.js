// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines various strategies for recovering automation nodes.
 */

const AutomationNode = chrome.automation.AutomationNode;
const RoleType = chrome.automation.RoleType;

export class RecoveryStrategy {
  /**
   * @param {!AutomationNode} node
   */
  constructor(node) {
    /** @private {!AutomationNode} */
    this.node_ = node;
  }

  /** @return {!AutomationNode} */
  get node() {
    if (this.requiresRecovery()) {
      this.node_ = this.recover() || this.node_;
    }

    return this.node_;
  }

  /** @return {boolean} */
  requiresRecovery() {
    return !this.node_ || !this.node_.role;
  }

  /**
   * @return {AutomationNode}
   * @protected
   */
  recover() {
    return null;
  }

  equalsWithoutRecovery(rhs) {
    return this.node_ === rhs.node_;
  }
}


/**
 * A recovery strategy that uses the node's ancestors.
 */
export class AncestryRecoveryStrategy extends RecoveryStrategy {
  constructor(node) {
    super(node);

    /** @type {!Array<AutomationNode>} @private */
    this.ancestry_ = [];
    let nodeWalker = node;
    while (nodeWalker) {
      this.ancestry_.push(nodeWalker);
      nodeWalker = nodeWalker.parent;
      if (nodeWalker && nodeWalker.role === RoleType.WINDOW) {
        break;
      }
    }
  }

  /** @override */
  recover() {
    return this.ancestry_[this.getFirstValidNodeIndex_()];
  }

  /**
   * @return {number}
   * @protected
   */
  getFirstValidNodeIndex_() {
    for (let i = 0; i < this.ancestry_.length; i++) {
      const firstValidNode = this.ancestry_[i];
      if (firstValidNode != null && firstValidNode.role !== undefined &&
          firstValidNode.root !== undefined) {
        return i;
      }
    }
    return 0;
  }
}


/**
 * A recovery strategy that uses the node's tree path.
 */
export class TreePathRecoveryStrategy extends AncestryRecoveryStrategy {
  constructor(node) {
    super(node);

    /** @type {!Array<number>} @private */
    this.recoveryChildIndex_ = [];
    let nodeWalker = node;
    while (nodeWalker) {
      this.recoveryChildIndex_.push(nodeWalker.indexInParent);
      nodeWalker = nodeWalker.parent;
      if (nodeWalker && nodeWalker.role === RoleType.WINDOW) {
        break;
      }
    }
  }

  /** @override */
  recover() {
    const index = this.getFirstValidNodeIndex_();
    if (index === 0) {
      return this.ancestry_[index];
    }

    // Otherwise, attempt to recover.
    let node = this.ancestry_[index];
    for (let j = index - 1; j >= 0; j--) {
      const childIndex = this.recoveryChildIndex_[j];
      const children = node.children;
      if (!children[childIndex]) {
        return node;
      }
      node = children[childIndex];
    }
    return node;
  }
}
