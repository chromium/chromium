// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationPredicate} from '../../common/automation_predicate.js';
import {EventGenerator} from '../../common/event_generator.js';
import {KeyCode} from '../../common/key_code.js';
import {RepeatedEventHandler} from '../../common/repeated_event_handler.js';
import {Navigator} from '../navigator.js';
import {SAConstants, SwitchAccessMenuAction} from '../switch_access_constants.js';

import {BasicNode} from './basic_node.js';
import {SAChildNode, SARootNode} from './switch_access_node.js';

const AutomationNode = chrome.automation.AutomationNode;

/**
 * This class handles interactions with combo boxes.
 * TODO(anastasi): Add a test for this class.
 */
class ComboBoxNode extends BasicNode {
  /**
   * @param {!AutomationNode} baseNode
   * @param {?SARootNode} parent
   */
  constructor(baseNode, parent) {
    super(baseNode, parent);

    /** @private {?RepeatedEventHandler} */
    this.expandedChangedHandler_;
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
    super.onFocus();

    this.expandedChangedHandler_ = new RepeatedEventHandler(
        this.automationNode, chrome.automation.EventType.EXPANDED,
        () => this.onExpandedChanged(), {exactMatch: true});
  }

  /** @override */
  onUnfocus() {
    super.onUnfocus();

    if (this.expandedChangedHandler_) {
      this.expandedChangedHandler_.stop();
      this.expandedChangedHandler_ = null;
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
        EventGenerator.sendKeyPress(KeyCode.UP);
        return SAConstants.ActionResponse.REMAIN_OPEN;
      case SwitchAccessMenuAction.INCREMENT:
        EventGenerator.sendKeyPress(KeyCode.DOWN);
        return SAConstants.ActionResponse.REMAIN_OPEN;
    }
    return super.performAction(action);
  }

  onExpandedChanged() {
    // TODO: figure out why a short timeout is needed here.
    setTimeout(() => {
      if (this.isGroup()) {
        Navigator.byItem.enterGroup();
      }
    }, 250);
  }
}

BasicNode.creators.push({
  predicate: AutomationPredicate.comboBox,
  creator: (node, parent) => new ComboBoxNode(node, parent),
});
