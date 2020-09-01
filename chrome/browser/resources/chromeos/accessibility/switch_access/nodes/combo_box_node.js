// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles interactions with combo boxes.
 * TODO(anastasi): Add a test for this class.
 */
class ComboBoxNode extends NodeWrapper {
  /**
   * @param {!AutomationNode} baseNode
   * @param {?SARootNode} parent
   */
  constructor(baseNode, parent) {
    super(baseNode, parent);

    /** @private {?RepeatedEventHandler} */
    this.childrenChangedHandler_;
  }

  /** @override */
  get actions() {
    const actions = super.actions;
    if (!actions.includes(SwitchAccessMenuAction.INCREMENT) &&
        !actions.includes(SwitchAccessMenuAction.DECREMENT)) {
      actions.push(
          SwitchAccessMenuAction.INCREMENT, SwitchAccessMenuAction.DECREMENT);
    }
    return actions;
  }

  /** @override */
  onFocus() {
    if (this.automationNode) {
      this.childrenChangedHandler_ = new RepeatedEventHandler(
          this.automationNode, chrome.automation.EventType.CHILDREN_CHANGED,
          () => this.onChildrenChanged(), {exactMatch: true});
    }

    super.onFocus();
    this.automationNode.focus();
  }

  /** @override */
  onUnfocus() {
    super.onUnfocus();

    if (this.childrenChangedHandler_) {
      this.childrenChangedHandler_.stopListening();
      this.childrenChangedHandler_ = null;
    }
  }

  /** @override */
  performAction(action) {
    // The box of options that typically pops up with combo boxes is not
    // currently given a location in the automation tree, so we work around that
    // by selecting a value without opening the pop-up, using the up and down
    // arrows.
    switch (action) {
      case SwitchAccessMenuAction.DECREMENT:
        EventHelper.simulateKeyPress(EventHelper.KeyCode.UP_ARROW);
        return SAConstants.ActionResponse.REMAIN_OPEN;
      case SwitchAccessMenuAction.INCREMENT:
        EventHelper.simulateKeyPress(EventHelper.KeyCode.DOWN_ARROW);
        return SAConstants.ActionResponse.REMAIN_OPEN;
    }
    return super.performAction(action);
  }

  onChildrenChanged() {
    // TODO: figure out why a short timeout is needed here.
    window.setTimeout(() => {
      if (this.isGroup()) {
        NavigationManager.enterGroup();
      }
    }, 250);
  }
}
