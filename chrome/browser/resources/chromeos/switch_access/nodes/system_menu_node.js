// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class represents the group rooted at the system menu.
 */
class SystemMenuRootNode extends RootNodeWrapper {
  /**
   * @param {!chrome.automation.AutomationNode} menuNode
   * @private
   */
  constructor(menuNode) {
    super(menuNode);
  }

  /** @override */
  onExit() {
    // To close a system menu, we need to send an escape key event.
    EventHelper.simulateKeyPress(EventHelper.KeyCode.ESC);
  }

  /**
   * Creates the tree structure for the system menu.
   * @param {!chrome.automation.AutomationNode} menuNode
   * @return {!SystemMenuRootNode}
   */
  static buildTree(menuNode) {
    const root = new SystemMenuRootNode(menuNode);
    const childConstructor = (node) => new NodeWrapper(node, root);

    RootNodeWrapper.findAndSetChildren(root, childConstructor);
    return root;
  }
}
