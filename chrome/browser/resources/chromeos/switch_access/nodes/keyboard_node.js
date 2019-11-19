// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles the behavior of keyboard nodes directly associated with a
 * single AutomationNode.
 */
class KeyboardNode extends NodeWrapper {
  /**
   * @param {!chrome.automation.AutomationNode} node
   * @param {!SARootNode} parent
   */
  constructor(node, parent) {
    super(node, parent);
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    if (this.isGroup()) {
      return [];
    }
    return [SAConstants.MenuAction.SELECT];
  }

  // ================= General methods =================

  /** @override */
  asRootNode() {
    if (!this.isGroup()) {
      return null;
    }

    const node = this.automationNode;
    if (!node) {
      throw new TypeError('Keyboard nodes must have an automation node.');
    }

    const root = new RootNodeWrapper(node);
    KeyboardNode.findAndSetChildren(root);
    return root;
  }

  /** @override */
  performAction(action) {
    if (this.isGroup()) {
      return false;
    }
    if (action !== SAConstants.MenuAction.SELECT) {
      return false;
    }
    let keyLocation = this.location;
    if (!keyLocation) {
      return false;
    }

    // doDefault() does nothing on Virtual Keyboard buttons, so we must
    // simulate a mouse click.
    const center = RectHelper.center(keyLocation);
    EventHelper.simulateMouseClick(
        center.x, center.y, SAConstants.VK_KEY_PRESS_DURATION_MS);

    return true;
  }

  // ================= Static methods =================

  /**
   * Helper function to connect tree elements, given the root node.
   * @param {!RootNodeWrapper} root
   */
  static findAndSetChildren(root) {
    const childConstructor = (node) => new KeyboardNode(node, root);

    /** @type {!Array<!chrome.automation.AutomationNode>} */
    let interestingChildren = RootNodeWrapper.getInterestingChildren(root);
    let children = interestingChildren.map(childConstructor);
    if (interestingChildren.length > SAConstants.KEYBOARD_MAX_ROW_LENGTH) {
      children = GroupNode.separateByRow(children);
    }

    children.push(new BackButtonNode(root));
    root.children = children;
  }
}

/**
 * This class handles the top-level Keyboard node, as well as the construction
 * of the Keyboard tree.
 */
class KeyboardRootNode extends RootNodeWrapper {
  /**
   * @param {!chrome.automation.AutomationNode} keyboard
   * @private
   */
  constructor(keyboard) {
    super(keyboard);
  }

  // ================= General methods =================

  /** @override */
  onExit() {
    chrome.accessibilityPrivate.setVirtualKeyboardVisible(false);
  }

  // ================= Private methods =================

  /**
   * Custom logic when entering the node.
   */
  onEnter_() {
    chrome.accessibilityPrivate.setVirtualKeyboardVisible(true);
  }

  // ================= Static methods =================

  /**
   * Creates the tree structure for the system menu.
   * @param {!chrome.automation.AutomationNode} desktop
   * @return {!KeyboardRootNode}
   */
  static buildTree(desktop) {
    const keyboardContainer =
        desktop.find({role: chrome.automation.RoleType.KEYBOARD});
    const keyboard =
        new AutomationTreeWalker(keyboardContainer, constants.Dir.FORWARD, {
          visit: (node) => SwitchAccessPredicate.isGroup(node, null),
          root: (node) => node === keyboardContainer
        })
            .next()
            .node;

    const root = new KeyboardRootNode(keyboard);
    root.onEnter_();
    KeyboardNode.findAndSetChildren(root);
    return root;
  }
}
