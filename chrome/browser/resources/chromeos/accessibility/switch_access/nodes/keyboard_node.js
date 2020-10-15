// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles the behavior of keyboard nodes directly associated with a
 * single AutomationNode.
 */
class KeyboardNode extends BasicNode {
  /**
   * @param {!AutomationNode} node
   * @param {!SARootNode} parent
   */
  constructor(node, parent) {
    super(node, parent);
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    return [SwitchAccessMenuAction.SELECT];
  }

  // ================= General methods =================

  /** @override */
  asRootNode() {
    return null;
  }

  /** @override */
  isGroup() {
    return false;
  }

  /** @override */
  isValidAndVisible() {
    if (super.isValidAndVisible()) {
      return true;
    }
    if (!KeyboardNode.resetting &&
        NavigationManager.currentGroupHasChild(this)) {
      // TODO(crbug/1130773): move this code to another location, if possible
      KeyboardNode.resetting = true;
      KeyboardRootNode.ignoreNextExit_ = true;
      NavigationManager.exitKeyboard();
      NavigationManager.enterKeyboard();
    }

    return false;
  }

  /** @override */
  performAction(action) {
    if (action !== SwitchAccessMenuAction.SELECT) {
      return SAConstants.ActionResponse.NO_ACTION_TAKEN;
    }

    const keyLocation = this.location;
    if (!keyLocation) {
      return SAConstants.ActionResponse.NO_ACTION_TAKEN;
    }

    // doDefault() does nothing on Virtual Keyboard buttons, so we must
    // simulate a mouse click.
    const center = RectUtil.center(keyLocation);
    EventGenerator.sendMouseClick(
        center.x, center.y, SAConstants.VK_KEY_PRESS_DURATION_MS);

    return SAConstants.ActionResponse.CLOSE_MENU;
  }
}

/**
 * This class handles the top-level Keyboard node, as well as the construction
 * of the Keyboard tree.
 */
class KeyboardRootNode extends BasicRootNode {
  /**
   * @param {!AutomationNode} groupNode
   * @private
   */
  constructor(groupNode) {
    super(groupNode);
    KeyboardNode.resetting = false;
  }

  // ================= General methods =================


  /** @override */
  isValidGroup() {
    // To ensure we can find the keyboard root node to appropriately respond to
    // visibility changes, never mark it as invalid.
    return true;
  }

  /** @override */
  onExit() {
    if (KeyboardRootNode.ignoreNextExit_) {
      KeyboardRootNode.ignoreNextExit_ = false;
      return;
    }

    // If the keyboard is currently visible, ignore the corresponding
    // state change.
    if (KeyboardRootNode.isVisible_) {
      KeyboardRootNode.explicitStateChange_ = true;
      chrome.accessibilityPrivate.setVirtualKeyboardVisible(false);
    }

    AutoScanManager.setInKeyboard(false);
  }

  /** @override */
  refreshChildren() {
    KeyboardRootNode.findAndSetChildren_(this);
  }

  // ================= Static methods =================

  /**
   * Creates the tree structure for the system menu.
   * @return {!KeyboardRootNode}
   */
  static buildTree() {
    KeyboardRootNode.loadKeyboard_();
    AutoScanManager.setInKeyboard(true);

    const keyboard = KeyboardRootNode.getKeyboardObject();
    if (!keyboard) {
      throw SwitchAccess.error(
          SAConstants.ErrorType.MISSING_KEYBOARD,
          'Could not find keyboard in the automation tree',
          true /* shouldRecover */);
    }
    const root = new KeyboardRootNode(keyboard);
    KeyboardRootNode.findAndSetChildren_(root);
    return root;
  }

  /**
   * Start listening for keyboard open/closed.
   */
  static startWatchingVisibility() {
    const keyboardObject = KeyboardRootNode.getKeyboardObject();
    if (!keyboardObject) {
      SwitchAccess.findNodeMatching(
          {role: chrome.automation.RoleType.KEYBOARD},
          KeyboardRootNode.startWatchingVisibility);
      return;
    }

    KeyboardRootNode.isVisible_ =
        SwitchAccessPredicate.isVisible(keyboardObject);

    new EventHandler(
        keyboardObject, chrome.automation.EventType.STATE_CHANGED,
        KeyboardRootNode.checkVisibilityChanged_, {exactMatch: true})
        .start();
  }

  // ================= Private static methods =================

  /**
   * @param {chrome.automation.AutomationEvent} event
   * @private
   */
  static checkVisibilityChanged_(event) {
    const currentlyVisible =
        SwitchAccessPredicate.isVisible(KeyboardRootNode.getKeyboardObject());
    if (currentlyVisible === KeyboardRootNode.isVisible_) {
      return;
    }

    KeyboardRootNode.isVisible_ = currentlyVisible;

    if (KeyboardRootNode.explicitStateChange_) {
      // When the user has explicitly shown / hidden the keyboard, do not
      // enter / exit the keyboard again to avoid looping / double-calls.
      KeyboardRootNode.explicitStateChange_ = false;
      return;
    }

    if (KeyboardRootNode.isVisible_) {
      NavigationManager.enterKeyboard();
    } else {
      NavigationManager.exitKeyboard();
    }
  }

  /**
   * Helper function to connect tree elements, given the root node.
   * @param {!KeyboardRootNode} root
   * @private
   */
  static findAndSetChildren_(root) {
    const childConstructor = (node) => new KeyboardNode(node, root);
    const interestingChildren =
        root.automationNode.findAll({role: chrome.automation.RoleType.BUTTON});
    /** @type {!Array<!SAChildNode>} */
    const children = GroupNode.separateByRow(
        interestingChildren.map(childConstructor), root.automationNode);

    children.push(new BackButtonNode(root));
    root.children = children;
  }

  /**
   * @return {AutomationNode}
   * @private
   */
  static getKeyboardObject() {
    if (!this.object_ || !this.object_.role) {
      this.object_ = NavigationManager.desktopNode.find(
          {role: chrome.automation.RoleType.KEYBOARD});
    }
    return this.object_;
  }

  /**
   * Loads the keyboard.
   * @private
   */
  static loadKeyboard_() {
    if (KeyboardRootNode.isVisible_) {
      return;
    }

    KeyboardRootNode.explicitStateChange_ = true;
    chrome.accessibilityPrivate.setVirtualKeyboardVisible(true);
  }
}
