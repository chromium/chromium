// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RectUtil} from '../../common/rect_util.js';
import {Navigator} from '../navigator.js';
import {ActionResponse} from '../switch_access_constants.js';

import {BackButtonNode} from './back_button_node.js';
import {BasicNode, BasicRootNode} from './basic_node.js';
import {SAChildNode, SARootNode} from './switch_access_node.js';

const AutomationNode = chrome.automation.AutomationNode;
const MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;

/**
 * This class handles the behavior of tab nodes at the top level (i.e. as
 * groups).
 */
export class TabNode extends BasicNode {
  /**
   * @param {!AutomationNode} node The node in the automation
   *    tree
   * @param {?SARootNode} parent
   * @param {!SARootNode} tabAsRoot A pre-calculated object for exploring the
   * parts of tab (i.e. choosing whether to open the tab or close it).
   */
  constructor(node, parent, tabAsRoot) {
    super(node, parent);

    /** @private {!SARootNode} */
    this.tabAsRoot_ = tabAsRoot;
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    return [MenuAction.DRILL_DOWN];
  }

  // ================= General methods =================

  /** @override */
  asRootNode() {
    return this.tabAsRoot_;
  }

  /** @override */
  isGroup() {
    return true;
  }

  /** @override */
  performAction(action) {
    if (action !== MenuAction.DRILL_DOWN) {
      return ActionResponse.NO_ACTION_TAKEN;
    }
    Navigator.byItem.enterGroup();
    return ActionResponse.CLOSE_MENU;
  }

  // ================= Static methods =================

  /** @override */
  static create(tabNode, parent) {
    const tabAsRoot = new BasicRootNode(tabNode);

    let closeButton;
    for (const child of tabNode.children) {
      if (child.role === chrome.automation.RoleType.BUTTON) {
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
  /**
   * @param {!AutomationNode} node
   * @param {?SARootNode} parent
   * @param {?SAChildNode} closeButton
   */
  constructor(node, parent, closeButton) {
    super(node, parent);

    /** @private {?SAChildNode} */
    this.closeButton_ = closeButton;
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    return [MenuAction.SELECT];
  }

  /** @override */
  get location() {
    if (!this.closeButton_) {
      return super.location;
    }
    return RectUtil.difference(super.location, this.closeButton_.location);
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
}

BasicNode.creators.push({
  predicate: baseNode => baseNode.role === chrome.automation.RoleType.TAB &&
      baseNode.root.role === chrome.automation.RoleType.DESKTOP,
  creator: TabNode.create,
});
