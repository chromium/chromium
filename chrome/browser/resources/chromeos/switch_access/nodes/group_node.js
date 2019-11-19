// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles the grouping of nodes that are not grouped in the
 *     automation tree. They are defined by their parent and child nodes.
 * Ex: Nodes in the virtual keyboard have no intermediate grouping, but should
 *     be grouped by row.
 */
class GroupNode extends SAChildNode {
  /**
   * @param {!Array<!SAChildNode>} children The nodes that this group contains.
   *     Should not include the back button.
   * @private
   */
  constructor(children) {
    super();

    /** @type {!Array<!SAChildNode>} */
    this.children_ = children;
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    return [];
  }

  /** @override */
  get automationNode() {
    return null;
  }

  /** @override */
  get location() {
    const childLocations = this.children_.map(c => c.location);
    return RectHelper.unionAll(childLocations);
  }

  /** @override */
  get role() {
    return chrome.automation.RoleType.GROUP;
  }

  // ================= General methods =================

  /** @override */
  asRootNode() {
    const root = new SARootNode();

    let children = [];
    for (const child of this.children_) {
      children.push(child);
    }

    children.push(new BackButtonNode(root));
    root.children = children;

    return root;
  }

  /** @override */
  equals(other) {
    if (!(other instanceof GroupNode)) {
      return false;
    }

    other = /** @type {GroupNode} */ (other);
    if (other.children_.length !== this.children_.length) {
      return false;
    }
    for (let i = 0; i < this.children_.length; i++) {
      if (other.children_[i].equals(this.children_[i])) {
        return false;
      }
    }
    return true;
  }

  /** @override */
  isEquivalentTo(node) {
    return false;
  }

  /** @override */
  isGroup() {
    return true;
  }

  /** @override */
  performAction(action) {
    return true;
  }

  // ================= Static methods =================

  /**
   * Assumes nodes are visually in rows.
   * @param {!Array<!SAChildNode>} nodes
   * @return {!Array<!GroupNode>}
   */
  static separateByRow(nodes) {
    let result = [];

    for (let i = 0; i < nodes.length;) {
      let children = [];
      children.push(nodes[i]);
      i++;

      while (i < nodes.length &&
             RectHelper.sameRow(children[0].location, nodes[i].location)) {
        children.push(nodes[i]);
        i++;
      }
      if (children.length <= 1) {
        throw new Error('Cannot group row with only one element.');
      }

      result.push(new GroupNode(children));
    }

    return result;
  }
}
