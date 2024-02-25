// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventHandler} from '/common/event_handler.js';
import {RepeatedEventHandler} from '/common/repeated_event_handler.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {ActionManager} from '../action_manager.js';
import {FocusRingManager} from '../focus_ring_manager.js';
import {MenuManager} from '../menu_manager.js';
import {Navigator} from '../navigator.js';
import {SwitchAccess} from '../switch_access.js';
import {ActionResponse} from '../switch_access_constants.js';

import {SAChildNode, SARootNode} from './switch_access_node.js';

type AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
import MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;
type Rect = chrome.automation.Rect;
import RoleType = chrome.automation.RoleType;

/**
 * This class handles the behavior of the back button.
 */
export class BackButtonNode extends SAChildNode {
  /** The group that the back button is shown for. */
  private group_: SARootNode;
  private locationChangedHandler_?: RepeatedEventHandler;

  private static automationNode_: AutomationNode;
  private static clickHandler_: EventHandler;
  static locationForTesting?: Rect;

  constructor(group: SARootNode) {
    super();
    this.group_ = group;
  }

  // ================= Getters and setters =================

  override get actions(): MenuAction[] {
    return [MenuAction.SELECT];
  }

  override get automationNode(): AutomationNode {
    return BackButtonNode.automationNode_;
  }

  override get group(): SARootNode {
    return this.group_;
  }

  override get location(): Rect | undefined {
    if (BackButtonNode.locationForTesting) {
      return BackButtonNode.locationForTesting;
    }
    if (this.automationNode) {
      return this.automationNode.location;
    }
    return undefined;
  }

  override get role(): RoleType {
    return RoleType.BUTTON;
  }

  // ================= General methods =================

  override asRootNode(): SARootNode | undefined {
    return undefined;
  }

  override equals(other: SAChildNode): boolean {
    return other instanceof BackButtonNode;
  }

  override isEquivalentTo(
      node: SAChildNode | SARootNode | AutomationNode | null | undefined)
      : boolean {
    return node instanceof BackButtonNode || this.automationNode === node;
  }

  override isGroup(): boolean {
    return false;
  }

  override isValidAndVisible(): boolean {
    return this.group_.isValidGroup();
  }

  override onFocus(): void {
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

  override onUnfocus(): void {
    super.onUnfocus();
    chrome.accessibilityPrivate.updateSwitchAccessBubble(
        chrome.accessibilityPrivate.SwitchAccessBubble.BACK_BUTTON,
        false /* show */);

    if (this.locationChangedHandler_) {
      this.locationChangedHandler_.stop();
    }
  }

  override performAction(action: MenuAction): ActionResponse {
    if (action === MenuAction.SELECT && this.automationNode) {
      BackButtonNode.onClick_();
      return ActionResponse.CLOSE_MENU;
    }
    return ActionResponse.NO_ACTION_TAKEN;
  }

  override ignoreWhenComputingUnionOfBoundingBoxes(): boolean {
    return true;
  }

  // ================= Debug methods =================

  override debugString(
      wholeTree: boolean, prefix = '', currentNode = null): string {
    if (!this.automationNode) {
      return 'BackButtonNode';
    }
    return super.debugString(wholeTree, prefix, currentNode);
  }

  // ================= Static methods =================

  /** Looks for the back button automation node. */
  private static findAutomationNode_(): void {
    if (BackButtonNode.automationNode_ && BackButtonNode.automationNode_.role) {
      return;
    }
    SwitchAccess.findNodeMatching(
        {
          role: RoleType.BUTTON,
          attributes: {className: 'SwitchAccessBackButtonView'},
        },
        BackButtonNode.saveAutomationNode_);
  }

  /**
   * This function defines the behavior that should be taken when the back
   * button is pressed.
   */
  private static onClick_(): void {
    if (MenuManager.isMenuOpen()) {
      ActionManager.exitCurrentMenu();
    } else {
      Navigator.byItem.exitGroupUnconditionally();
    }
  }

  /** Saves the back button automation node. */
  private static saveAutomationNode_(automationNode: AutomationNode): void {
    BackButtonNode.automationNode_ = automationNode;

    if (BackButtonNode.clickHandler_) {
      BackButtonNode.clickHandler_.setNodes(automationNode);
    } else {
      BackButtonNode.clickHandler_ = new EventHandler(
          automationNode, EventType.CLICKED,
          BackButtonNode.onClick_);
    }
  }
}

TestImportManager.exportForTesting(BackButtonNode);
