// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RectUtil} from '/common/rect_util.js';

import {Navigator} from '../navigator.js';
import {ActionResponse} from '../switch_access_constants.js';

import {BackButtonNode} from './back_button_node.js';
import {BasicNode, BasicRootNode} from './basic_node.js';
import type {SAChildNode, SARootNode} from './switch_access_node.js';

type AutomationNode = chrome.automation.AutomationNode;
import MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;
type Rect = chrome.automation.Rect;
import RoleType = chrome.automation.RoleType;

/**
 * This class handles the behavior of tab nodes at the top level (i.e. as
 * groups).
 */
export class TabNode extends BasicNode {
  /**
   * @param node The node in the automation tree
   * @param tabAsRoot A pre-calculated object for exploring the parts of tab
   * (i.e. choosing whether to open the tab or close it).
   */
  constructor(
      node: AutomationNode, parent: SARootNode|null,
      private tabAsRoot_: SARootNode) {
    super(node, parent);
  }

  // ================= Getters and setters =================

  override get actions(): MenuAction[] {
    return [MenuAction.DRILL_DOWN];
  }

  // ================= General methods =================

  override asRootNode(): SARootNode {
    return this.tabAsRoot_;
  }

  override isGroup(): boolean {
    return true;
  }

  override performAction(action: MenuAction): ActionResponse {
    if (action !== MenuAction.DRILL_DOWN) {
      return ActionResponse.NO_ACTION_TAKEN;
    }
    Navigator.byItem.enterGroup();
    return ActionResponse.CLOSE_MENU;
  }

  // ================= Static methods =================

  static override create(tabNode: AutomationNode, parent: SARootNode|null):
      BasicNode {
    const tabAsRoot = new BasicRootNode(tabNode);

    let closeButton;
    for (const child of tabNode.children) {
      if (child.role === RoleType.BUTTON) {
        closeButton = new BasicNode(child, tabAsRoot);
        break;
      }
    }
    if (!closeButton) {
      // Pinned tabs have no close button, and so can be treated as just
      // actionable.
      return new ActionableTabNode(tabNode, parent, null);
    }

    const tabToSelect = new ActionableTabNode(tabNode, tabAsRoot, closeButton);
    const backButton = new BackButtonNode(tabAsRoot);
    tabAsRoot.children = [tabToSelect, closeButton, backButton];

    return new TabNode(tabNode, parent, tabAsRoot);
  }
}

/** This class handles the behavior of tabs as actionable elements */
class ActionableTabNode extends BasicNode {
  constructor(
      node: AutomationNode, parent: SARootNode|null,
      private closeButton_: SAChildNode|null) {
    super(node, parent);
  }

  // ================= Getters and setters =================

  override get actions(): MenuAction[] {
    return [MenuAction.SELECT];
  }

  override get location(): Rect|undefined {
    if (!this.closeButton_) {
      return super.location;
    }
    return RectUtil.difference(super.location, this.closeButton_.location);
  }

  // ================= General methods =================

  override asRootNode(): SARootNode|undefined {
    return undefined;
  }

  override isGroup(): boolean {
    return false;
  }
}

// TODO(crbug.com/314203187): Not null asserted, check that this is correct.
BasicNode.creators.push({
  predicate: baseNode => baseNode.role === RoleType.TAB &&
      baseNode.root!.role === RoleType.DESKTOP,
  creator: TabNode.create,
});
