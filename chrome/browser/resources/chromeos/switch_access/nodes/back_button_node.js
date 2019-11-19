// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles the behavior of the back button.
 */
class BackButtonNode extends SAChildNode {
  /**
   * @param {!SARootNode} group
   */
  constructor(group) {
    super();
    /**
     * The group that the back button is shown for.
     * @private {!SARootNode}
     */
    this.group_ = group;

    /** @private {chrome.automation.AutomationNode} */
    this.node_ = SwitchAccess.get().getBackButtonAutomationNode();
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    return [SAConstants.MenuAction.SELECT];
  }

  /** @override */
  get automationNode() {
    return this.node_;
  }

  /** @override */
  get location() {
    if (this.node_) {
      return this.node_.location;
    }
  }

  /** @override */
  get role() {
    return chrome.automation.RoleType.BUTTON;
  }

  // ================= General methods =================

  /** @override */
  asRootNode() {
    return null;
  }

  /** @override */
  equals(other) {
    return other instanceof BackButtonNode;
  }

  /** @override */
  isEquivalentTo(node) {
    return this.node_ === node;
  }

  /** @override */
  isGroup() {
    return false;
  }

  /** @override */
  onFocus() {
    chrome.accessibilityPrivate.setSwitchAccessMenuState(
        true, this.group_.location, 0 /* num_actions */);
  }

  /** @override */
  onUnfocus() {
    chrome.accessibilityPrivate.setSwitchAccessMenuState(
        false, RectHelper.ZERO_RECT, 0 /* num_actions */);
  }

  /** @override */
  performAction(action) {
    if (action !== SAConstants.MenuAction.SELECT) {
      return false;
    }

    if (this.node_) {
      this.node_.doDefault();
    }
    return true;
  }

  // ================= Debug methods =================

  /** @override */
  debugString() {
    return 'BackButtonNode';
  }
}
