// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationPredicate} from '/common/automation_predicate.js';
import {EventGenerator} from '/common/event_generator.js';
import {KeyCode} from '/common/key_code.js';
import {RepeatedEventHandler} from '/common/repeated_event_handler.js';

import {Navigator} from '../navigator.js';
import {ActionResponse} from '../switch_access_constants.js';

import {BasicNode} from './basic_node.js';
import {SARootNode} from './switch_access_node.js';

type AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
import MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;

/**
 * This class handles interactions with combo boxes.
 * TODO(anastasi): Add a test for this class.
 */
class ComboBoxNode extends BasicNode {
  private expandedChangedHandler_?: RepeatedEventHandler | null;

  constructor(baseNode: AutomationNode, parent: SARootNode | null) {
    super(baseNode, parent);
  }

  override get actions(): MenuAction[] {
    const actions = super.actions;
    if (!actions.includes(MenuAction.INCREMENT) &&
        !actions.includes(MenuAction.DECREMENT)) {
      actions.push(MenuAction.INCREMENT, MenuAction.DECREMENT);
    }
    return actions;
  }

  override onFocus(): void {
    super.onFocus();

    this.expandedChangedHandler_ = new RepeatedEventHandler(
        this.automationNode, EventType.EXPANDED,
        () => this.onExpandedChanged(), {exactMatch: true});
  }

  override onUnfocus(): void {
    super.onUnfocus();

    if (this.expandedChangedHandler_) {
      this.expandedChangedHandler_.stop();
      this.expandedChangedHandler_ = null;
    }
  }

  override performAction(action: MenuAction): ActionResponse {
    // The box of options that typically pops up with combo boxes is not
    // currently given a location in the automation tree, so we work around that
    // by selecting a value without opening the pop-up, using the up and down
    // arrows.
    switch (action) {
      case MenuAction.DECREMENT:
        EventGenerator.sendKeyPress(KeyCode.UP);
        return ActionResponse.REMAIN_OPEN;
      case MenuAction.INCREMENT:
        EventGenerator.sendKeyPress(KeyCode.DOWN);
        return ActionResponse.REMAIN_OPEN;
    }
    return super.performAction(action);
  }

  onExpandedChanged(): void {
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
