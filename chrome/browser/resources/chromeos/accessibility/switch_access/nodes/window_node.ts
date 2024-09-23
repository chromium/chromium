// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SwitchAccessPredicate} from '../switch_access_predicate.js';

import {BasicNode, BasicRootNode} from './basic_node.js';

type AutomationNode = chrome.automation.AutomationNode;
const RoleType = chrome.automation.RoleType;

/** This class represents a window. */
export class WindowRootNode extends BasicRootNode {
  override onFocus(): void {
    super.onFocus();

    let focusNode = this.automationNode;
    // TODO(b/314203187): Not null asserted, check that this is correct.
    while (focusNode.className !== 'BrowserFrame' &&
           focusNode.parent!.role === RoleType.WINDOW) {
      focusNode = focusNode.parent!;
    }
    focusNode.focus();
  }

  /** Creates the tree structure for a window node. */
  static override buildTree(windowNode: AutomationNode): WindowRootNode {
    const root = new WindowRootNode(windowNode);
    const childConstructor =
        (node: AutomationNode): BasicNode => BasicNode.create(node, root);

    BasicRootNode.findAndSetChildren(root, childConstructor);
    return root;
  }
}

BasicRootNode.builders.push({
  predicate: (rootNode: AutomationNode) =>
      SwitchAccessPredicate.isWindow(rootNode),
  builder: WindowRootNode.buildTree,
});
