// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventGenerator} from '../../common/event_generator.js';
import {KeyCode} from '../../common/key_code.js';
import {ActionResponse} from '../switch_access_constants.js';

import {BasicNode} from './basic_node.js';
import {SAChildNode, SARootNode} from './switch_access_node.js';

const AutomationNode = chrome.automation.AutomationNode;
const MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;

/** This class handles interactions with sliders. */
export class SliderNode extends BasicNode {
  /**
   * @param {!AutomationNode} baseNode
   * @param {?SARootNode} parent
   */
  constructor(baseNode, parent) {
    super(baseNode, parent);
    this.isCustomSlider_ = true;
  }

  /** @override */
  onFocus() {
    super.onFocus();
    this.automationNode.focus();
  }

  /** @override */
  performAction(action) {
    // Currently, custom sliders have no way to support increment/decrement via
    // the automation API. We handle this case by simulating left/right arrow
    // presses.
    if (this.isCustomSlider_) {
      if (action === MenuAction.INCREMENT) {
        EventGenerator.sendKeyPress(KeyCode.RIGHT);
        return ActionResponse.REMAIN_OPEN;
      } else if (action === MenuAction.DECREMENT) {
        EventGenerator.sendKeyPress(KeyCode.LEFT);
        return ActionResponse.REMAIN_OPEN;
      }
    }

    return super.performAction(action);
  }
}

BasicNode.creators.push({
  predicate: baseNode => baseNode.role === chrome.automation.RoleType.SLIDER,
  creator: (node, parent) => new SliderNode(node, parent),
});
