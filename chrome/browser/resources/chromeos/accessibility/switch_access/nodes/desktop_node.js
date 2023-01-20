// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Navigator} from '../navigator.js';
import {SwitchAccess} from '../switch_access.js';
import {ErrorType} from '../switch_access_constants.js';
import {SwitchAccessPredicate} from '../switch_access_predicate.js';

import {BasicNode, BasicRootNode} from './basic_node.js';

const AutomationNode = chrome.automation.AutomationNode;

/**
 * This class handles interactions with the desktop automation node.
 */
export class DesktopNode extends BasicRootNode {
  /**
   * @param {!AutomationNode} autoNode The automation node representing the
   *     desktop.
   */
  constructor(autoNode) {
    super(autoNode);
  }

  // ================= General methods =================

  /** @override */
  equals(other) {
    // The underlying automation tree only has one desktop node, so all
    // DesktopNode instances are equal.
    return other instanceof DesktopNode;
  }

  /** @override */
  isValidGroup() {
    return true;
  }

  /** @override */
  refresh() {
    // Find the currently focused child.
    let focusedChild = null;
    for (const child of this.children) {
      if (child.isFocused()) {
        focusedChild = child;
        break;
      }
    }

    // Update this DesktopNode's children.
    const childConstructor = node => BasicNode.create(node, this);
    DesktopNode.findAndSetChildren(this, childConstructor);

    // Set the new instance of that child to be the focused node.
    for (const child of this.children) {
      if (child.isEquivalentTo(focusedChild)) {
        Navigator.byItem.forceFocusedNode(child);
        return;
      }
    }

    // If the previously focused node no longer exists, focus the first node in
    // the group.
    Navigator.byItem.forceFocusedNode(this.children[0]);
  }

  // ================= Static methods =================

  /**
   * @param {!AutomationNode} desktop
   * @return {!DesktopNode}
   */
  static build(desktop) {
    const root = new DesktopNode(desktop);
    const childConstructor = autoNode => BasicNode.create(autoNode, root);

    DesktopNode.findAndSetChildren(root, childConstructor);
    return root;
  }

  /** @override */
  static findAndSetChildren(root, childConstructor) {
    const interestingChildren = BasicRootNode.getInterestingChildren(root);

    if (interestingChildren.length < 1) {
      // If the desktop node does not behave as expected, we have no basis for
      // recovering. Wait for the next user input.
      throw SwitchAccess.error(
          ErrorType.MALFORMED_DESKTOP,
          'Desktop node must have at least 1 interesting child.',
          false /* shouldRecover */);
    }

    // TODO(crbug.com/1106080): Add hittest intervals to new children which are
    // SwitchAccessPredicate.isWindow to check whether those children are
    // occluded or visible. Remove any intervals on the previous window
    // children before reassigning root.children.
    root.children = interestingChildren.map(childConstructor);
  }
}
