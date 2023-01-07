// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SwitchAccessPredicate} from '../switch_access_predicate.js';

import {BasicNode, BasicRootNode} from './basic_node.js';

const AutomationNode = chrome.automation.AutomationNode;

/** This class represents a window. */
export class WindowRootNode extends BasicRootNode {
  /** @override */
  onFocus() {
    super.onFocus();

    let focusNode = this.automationNode;
    while (focusNode.className !== 'BrowserFrame' &&
           focusNode.parent.role === chrome.automation.RoleType.WINDOW) {
      focusNode = focusNode.parent;
    }
    focusNode.focus();
  }

  /**
   * Creates the tree structure for a window node.
   * @param {!AutomationNode} windowNode
   * @return {!WindowRootNode}
   */
  static buildTree(windowNode) {
    const root = new WindowRootNode(windowNode);
    const childConstructor = node => BasicNode.create(node, root);

    BasicRootNode.findAndSetChildren(root, childConstructor);
    return root;
  }
}

BasicRootNode.builders.push({
  predicate: rootNode => SwitchAccessPredicate.isWindow(rootNode),
  builder: WindowRootNode.buildTree,
});
