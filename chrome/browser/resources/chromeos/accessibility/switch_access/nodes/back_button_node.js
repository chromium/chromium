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

    /** @private {!RepeatedEventHandler} */
    this.locationChangedHandler_;
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    return [SwitchAccessMenuAction.SELECT];
  }

  /** @override */
  get automationNode() {
    return BackButtonNode.automationNode_;
  }

  /** @return {!SARootNode} */
  get group() {
    return this.group_;
  }

  /** @override */
  get location() {
    if (BackButtonNode.locationForTesting) {
      return BackButtonNode.locationForTesting;
    }
    if (this.automationNode) {
      return this.automationNode.location;
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
    return node instanceof BackButtonNode || this.automationNode === node;
  }

  /** @override */
  isGroup() {
    return false;
  }

  /** @override */
  isValidAndVisible() {
    return this.group_.isValidGroup();
  }

  /** @override */
  onFocus() {
    super.onFocus();
    chrome.accessibilityPrivate.updateSwitchAccessBubble(
        chrome.accessibilityPrivate.SwitchAccessBubble.BACK_BUTTON,
        true /* show */, this.group_.location);
    BackButtonNode.findAutomationNode_();

    this.locationChangedHandler_ = new RepeatedEventHandler(
        this.group_.automationNode,
        chrome.automation.EventType.LOCATION_CHANGED,
        () => FocusRingManager.setFocusedNode(this),
        {exactMatch: true, allAncestors: true});
  }

  /** @override */
  onUnfocus() {
    super.onUnfocus();
    chrome.accessibilityPrivate.updateSwitchAccessBubble(
        chrome.accessibilityPrivate.SwitchAccessBubble.BACK_BUTTON,
        false /* show */);

    if (this.locationChangedHandler_) {
      this.locationChangedHandler_.stop();
    }
  }

  /** @override */
  performAction(action) {
    if (action === SwitchAccessMenuAction.SELECT && this.automationNode) {
      BackButtonNode.onClick_();
      return SAConstants.ActionResponse.CLOSE_MENU;
    }
    return SAConstants.ActionResponse.NO_ACTION_TAKEN;
  }

  // ================= Debug methods =================

  /** @override */
  debugString(wholeTree, prefix = '', currentNode = null) {
    if (!this.automationNode) {
      return 'BackButtonNode';
    }
    return super.debugString(wholeTree, prefix, currentNode);
  }

  // ================= Static methods =================

  /**
   * Looks for the back button automation node.
   * @private
   */
  static findAutomationNode_() {
    if (BackButtonNode.automationNode_ && BackButtonNode.automationNode_.role) {
      return;
    }
    SwitchAccess.findNodeMatching(
        {
          role: chrome.automation.RoleType.BUTTON,
          attributes: {className: 'SwitchAccessBackButtonView'}
        },
        BackButtonNode.saveAutomationNode_);
  }

  /**
   * This function defines the behavior that should be taken when the back
   * button is pressed.
   * @private
   */
  static onClick_() {
    if (MenuManager.isMenuOpen()) {
      MenuManager.exit();
    } else {
      NavigationManager.exitGroupUnconditionally();
    }
  }

  /**
   * Saves the back button automation node.
   * @param {!AutomationNode} automationNode
   * @private
   */
  static saveAutomationNode_(automationNode) {
    BackButtonNode.automationNode_ = automationNode;

    if (BackButtonNode.clickHandler_) {
      BackButtonNode.clickHandler_.setNodes(automationNode);
    } else {
      BackButtonNode.clickHandler_ = new EventHandler(
          automationNode, chrome.automation.EventType.CLICKED,
          BackButtonNode.onClick_);
    }
  }
}
