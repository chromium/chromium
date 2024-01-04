// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines various strategies for recovering automation nodes.
 */

type AutomationNode = chrome.automation.AutomationNode;
import RoleType = chrome.automation.RoleType;

export abstract class RecoveryStrategy {
  private node_: AutomationNode;

  constructor(node: AutomationNode) {
    this.node_ = node;
  }

  get node(): AutomationNode {
    if (this.requiresRecovery()) {
      this.node_ = this.recover() || this.node_;
    }

    return this.node_;
  }

  requiresRecovery(): boolean {
    return !this.node_ || !this.node_.role;
  }

  protected abstract recover(): AutomationNode;

  equalsWithoutRecovery(rhs: RecoveryStrategy): boolean {
    return this.node_ === rhs.node_;
  }
}


/**
 * A recovery strategy that uses the node's ancestors.
 */
export class AncestryRecoveryStrategy extends RecoveryStrategy {
  protected ancestry_: AutomationNode[] = [];

  constructor(node: AutomationNode) {
    super(node);

    let nodeWalker: AutomationNode|undefined = node;
    while (nodeWalker) {
      this.ancestry_.push(nodeWalker);
      nodeWalker = nodeWalker.parent;
      if (nodeWalker && nodeWalker.role === RoleType.WINDOW) {
        break;
      }
    }
  }

  override recover(): AutomationNode {
    return this.ancestry_[this.getFirstValidNodeIndex_()];
  }

  protected getFirstValidNodeIndex_(): number {
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
  private recoveryChildIndex_: number[] = [];

  constructor(node: AutomationNode) {
    super(node);

    let nodeWalker: AutomationNode|undefined = node;
    while (nodeWalker) {
      // TODO(b/314203187): Not null asserted, check these to make sure this
      // is correct.
      this.recoveryChildIndex_.push(nodeWalker.indexInParent!);
      nodeWalker = nodeWalker.parent;
      if (nodeWalker && nodeWalker.role === RoleType.WINDOW) {
        break;
      }
    }
  }

  override recover(): AutomationNode {
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
