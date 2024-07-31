// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RectUtil} from '/common/rect_util.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Navigator} from '../navigator.js';
import {ActionResponse} from '../switch_access_constants.js';

import {BackButtonNode} from './back_button_node.js';
import {SAChildNode, SARootNode} from './switch_access_node.js';

type AutomationNode = chrome.automation.AutomationNode;
import MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;
type Rect = chrome.automation.Rect;
import RoleType = chrome.automation.RoleType;

/**
 * This class handles the grouping of nodes that are not grouped in the
 *     automation tree. They are defined by their parent and child nodes.
 * Ex: Nodes in the virtual keyboard have no intermediate grouping, but should
 *     be grouped by row.
 */
export class GroupNode extends SAChildNode {
  /**
   * @param children The nodes that this group contains.
   *     Should not include the back button.
   * @param containingNode The automation node most closely containing the
   *     children.
   */
  private constructor(
      private children_: SAChildNode[],
      private containingNode_: AutomationNode) {
    super();
  }

  // ================= Getters and setters =================

  override get actions(): MenuAction[] {
    return [MenuAction.DRILL_DOWN];
  }

  override get automationNode(): AutomationNode {
    return this.containingNode_;
  }

  override get location(): Rect {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    const childLocations =
        this.children_.filter(c => c.isValidAndVisible()).map(c => c.location!);
    return RectUtil.unionAll(childLocations);
  }

  override get role(): RoleType {
    return RoleType.GROUP;
  }

  // ================= General methods =================

  override asRootNode(): SARootNode {
    const root = new SARootNode(this.containingNode_);

    // Make a copy of the children array.
    const children = [...this.children_];

    children.push(new BackButtonNode(root));
    root.children = children;

    return root;
  }

  override equals(other: SAChildNode): boolean {
    if (!(other instanceof GroupNode)) {
      return false;
    }

    if (other.children_.length !== this.children_.length) {
      return false;
    }
    for (let i = 0; i < this.children_.length; i++) {
      if (!other.children_[i].equals(this.children_[i])) {
        return false;
      }
    }
    return true;
  }

  override isEquivalentTo(node: AutomationNode | SAChildNode | SARootNode):
      boolean {
    if (node instanceof GroupNode) {
      return this.equals(node);
    }

    for (const child of this.children_) {
      if (child.isEquivalentTo(node)) {
        return true;
      }
    }

    return false;
  }

  override isGroup(): boolean {
    return true;
  }

  override isValidAndVisible(): boolean {
    for (const child of this.children_) {
      if (child.isValidAndVisible()) {
        return super.isValidAndVisible();
      }
    }
    return false;
  }

  override performAction(action: MenuAction): ActionResponse {
    if (action === MenuAction.DRILL_DOWN) {
      Navigator.byItem.enterGroup();
      return ActionResponse.CLOSE_MENU;
    }
    return ActionResponse.NO_ACTION_TAKEN;
  }

  // ================= Static methods =================

  /** Assumes nodes are visually in rows. */
  static separateByRow(nodes: SAChildNode[], containingNode: AutomationNode):
      GroupNode[] {
    const result: GroupNode[] = [];

    for (let i = 0; i < nodes.length;) {
      const children: SAChildNode[] = [];
      children.push(nodes[i]);
      i++;

      while (i < nodes.length &&
             RectUtil.sameRow(children[0].location, nodes[i].location)) {
        children.push(nodes[i]);
        i++;
      }

      result.push(new GroupNode(children, containingNode));
    }

    return result;
  }
}

TestImportManager.exportForTesting(GroupNode);
