// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RectUtil} from '../../common/rect_util.js';
import {Navigator} from '../navigator.js';
import {SAConstants, SwitchAccessMenuAction} from '../switch_access_constants.js';

import {BackButtonNode} from './back_button_node.js';
import {SAChildNode, SARootNode} from './switch_access_node.js';

const AutomationNode = chrome.automation.AutomationNode;

/**
 * This class handles the grouping of nodes that are not grouped in the
 *     automation tree. They are defined by their parent and child nodes.
 * Ex: Nodes in the virtual keyboard have no intermediate grouping, but should
 *     be grouped by row.
 */
export class GroupNode extends SAChildNode {
  /**
   * @param {!Array<!SAChildNode>} children The nodes that this group contains.
   *     Should not include the back button.
   * @param {!AutomationNode} containingNode The automation node most closely
   * containing the children.
   * @private
   */
  constructor(children, containingNode) {
    super();

    /** @private {!Array<!SAChildNode>} */
    this.children_ = children;

    /** @private {!AutomationNode} */
    this.containingNode_ = containingNode;
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    return [SwitchAccessMenuAction.SELECT];
  }

  /** @override */
  get automationNode() {
    return this.containingNode_;
  }

  /** @override */
  get location() {
    const childLocations =
        this.children_.filter(c => c.isValidAndVisible()).map(c => c.location);
    return RectUtil.unionAll(childLocations);
  }

  /** @override */
  get role() {
    return chrome.automation.RoleType.GROUP;
  }

  // ================= General methods =================

  /** @override */
  asRootNode() {
    const root = new SARootNode(this.containingNode_);

    // Make a copy of the children array.
    const children = [...this.children_];

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
      if (!other.children_[i].equals(this.children_[i])) {
        return false;
      }
    }
    return true;
  }

  /** @override */
  isEquivalentTo(node) {
    if (node instanceof GroupNode) {
      return this.equals(node);
    }

    for (const child of this.children_) {
      if (child.isEquivalentTo(node)) {
        return true;
      }
    }

    return false;
  }

  /** @override */
  isGroup() {
    return true;
  }

  /** @override */
  isValidAndVisible() {
    for (const child of this.children_) {
      if (child.isValidAndVisible()) {
        return super.isValidAndVisible();
      }
    }
    return false;
  }

  /** @override */
  performAction(action) {
    if (action === SwitchAccessMenuAction.SELECT) {
      Navigator.byItem.enterGroup();
      return SAConstants.ActionResponse.CLOSE_MENU;
    }
    return SAConstants.ActionResponse.NO_ACTION_TAKEN;
  }

  // ================= Static methods =================

  /**
   * Assumes nodes are visually in rows.
   * @param {!Array<!SAChildNode>} nodes
   * @param {!AutomationNode} containingNode
   * @return {!Array<!GroupNode>}
   */
  static separateByRow(nodes, containingNode) {
    const result = [];

    for (let i = 0; i < nodes.length;) {
      const children = [];
      children.push(nodes[i]);
      i++;

      while (i < nodes.length &&
             RectUtil.sameRow(children[0].location, nodes[i].location)) {
        children.push(nodes[i]);
        i++;
      }

      result.push(new GroupNode(children, containingNode));
    }

    return result;
  }
}
