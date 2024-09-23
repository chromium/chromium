// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Navigator} from '../navigator.js';
import {SwitchAccess} from '../switch_access.js';
import {ErrorType} from '../switch_access_constants.js';

import {BasicNode, BasicRootNode} from './basic_node.js';
import {SAChildNode, SARootNode} from './switch_access_node.js';

type AutomationNode = chrome.automation.AutomationNode;

/**
 * This class handles interactions with the desktop automation node.
 */
export class DesktopNode extends BasicRootNode {
  // ================= General methods =================

  override equals(other: SARootNode): boolean {
    // The underlying automation tree only has one desktop node, so all
    // DesktopNode instances are equal.
    return other instanceof DesktopNode;
  }

  override isValidGroup(): boolean {
    return true;
  }

  override refresh(): void {
    // Find the currently focused child.
    let focusedChild: SAChildNode | null = null;
    for (const child of this.children) {
      if (child.isFocused()) {
        focusedChild = child;
        break;
      }
    }

    // Update this DesktopNode's children.
    const childConstructor = (node: AutomationNode): BasicNode =>
        BasicNode.create(node, this);
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

  static build(desktop: AutomationNode): DesktopNode {
    const root = new DesktopNode(desktop);
    const childConstructor = (autoNode: AutomationNode): BasicNode =>
        BasicNode.create(autoNode, root);

    DesktopNode.findAndSetChildren(root, childConstructor);
    return root;
  }

  static override findAndSetChildren(
      root: DesktopNode,
      childConstructor: (node: AutomationNode) => SAChildNode): void {
    const interestingChildren = BasicRootNode.getInterestingChildren(root);

    if (interestingChildren.length < 1) {
      // If the desktop node does not behave as expected, we have no basis for
      // recovering. Wait for the next user input.
      throw SwitchAccess.error(
          ErrorType.MALFORMED_DESKTOP,
          'Desktop node must have at least 1 interesting child.',
          false /* shouldRecover */);
    }

    // TODO(crbug.com/40706137): Add hittest intervals to new children which are
    // SwitchAccessPredicate.isWindow to check whether those children are
    // occluded or visible. Remove any intervals on the previous window
    // children before reassigning root.children.
    root.children = interestingChildren.map(childConstructor);
  }
}

TestImportManager.exportForTesting(DesktopNode);
