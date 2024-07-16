// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationPredicate} from '/common/automation_predicate.js';
import {constants} from '/common/constants.js';
import {RepeatedEventHandler} from '/common/repeated_event_handler.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';
import {AutomationTreeWalker} from '/common/tree_walker.js';

import {SACache} from '../cache.js';
import {FocusRingManager} from '../focus_ring_manager.js';
import {Navigator} from '../navigator.js';
import {SwitchAccess} from '../switch_access.js';
import {ActionResponse, ErrorType} from '../switch_access_constants.js';
import {SwitchAccessPredicate} from '../switch_access_predicate.js';

import {BackButtonNode} from './back_button_node.js';
import {SAChildNode, SARootNode} from './switch_access_node.js';

import AutomationActionType = chrome.automation.ActionType;
type AutomationNode = chrome.automation.AutomationNode;
import EventType = chrome.automation.EventType;
import MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;
type Rect = chrome.automation.Rect;
type RoleType = chrome.automation.RoleType;

interface Creator {
  predicate: AutomationPredicate.Unary;
  creator: (node: AutomationNode, parent: SARootNode | null) => BasicNode;
}

interface RootBuilder {
  predicate: AutomationPredicate.Unary;
  builder: (node: AutomationNode) => BasicRootNode;
}

/**
 * This class handles interactions with an onscreen element based on a single
 * AutomationNode.
 */
export class BasicNode extends SAChildNode {
  private baseNode_: AutomationNode;
  private parent_: SARootNode | null;
  private locationChangedHandler_?: RepeatedEventHandler;
  private isActionable_: boolean;
  private static creators_: Creator[] = [];

  protected constructor(baseNode: AutomationNode, parent: SARootNode | null) {
    super();
    this.baseNode_ = baseNode;
    this.parent_ = parent;
    this.isActionable_ = !this.isGroup() ||
        SwitchAccessPredicate.isActionable(baseNode, new SACache());
  }

  // ================= Getters and setters =================

  override get actions(): MenuAction[] {
    const actions: MenuAction[] = [];
    if (this.isActionable_) {
      actions.push(MenuAction.SELECT);
    }
    if (this.isGroup()) {
      actions.push(MenuAction.DRILL_DOWN);
    }

    const ancestor = this.getScrollableAncestor_();
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (ancestor.scrollable) {
      if (ancestor.scrollX! > ancestor.scrollXMin!) {
        actions.push(MenuAction.SCROLL_LEFT);
      }
      if (ancestor.scrollX! < ancestor.scrollXMax!) {
        actions.push(MenuAction.SCROLL_RIGHT);
      }
      if (ancestor.scrollY! > ancestor.scrollYMin!) {
        actions.push(MenuAction.SCROLL_UP);
      }
      if (ancestor.scrollY! < ancestor.scrollYMax!) {
        actions.push(MenuAction.SCROLL_DOWN);
      }
    }
    // Coerce enums to string arrays for comparison.
    const menuActions: string[] = Object.values(MenuAction);
    const standardActions: string[] = this.baseNode_.standardActions!
      .filter((action: string) => menuActions.includes(action));
    return actions.concat(standardActions as MenuAction[]);
  }

  override get automationNode(): AutomationNode {
    return this.baseNode_;
  }

  override get location(): Rect | undefined {
    return this.baseNode_.location;
  }

  override get role(): RoleType {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    return this.baseNode_.role!;
  }

  // ================= General methods =================

  override asRootNode(): SARootNode | undefined {
    if (!this.isGroup()) {
      return undefined;
    }
    return BasicRootNode.buildTree(this.baseNode_);
  }

  override equals(rhs: SAChildNode | null | undefined): boolean {
    if (!rhs || !(rhs instanceof BasicNode)) {
      return false;
    }

    const other = rhs as BasicNode;
    return other.baseNode_ === this.baseNode_;
  }

  override isEquivalentTo(
      node: AutomationNode | SAChildNode | SARootNode | null): boolean {
    if (node instanceof BasicNode) {
      return this.baseNode_ === node.baseNode_;
    }
    if (node instanceof BasicRootNode) {
      return this.baseNode_ === node.automationNode;
    }

    if (node instanceof SAChildNode) {
      return node.isEquivalentTo(this);
    }
    return this.baseNode_ === node;
  }

  override isGroup(): boolean {
    const cache = new SACache();
    return SwitchAccessPredicate.isGroup(this.baseNode_, this.parent_, cache);
  }

  override isValidAndVisible(): boolean {
    // Nodes may have been deleted or orphaned.
    if (!this.baseNode_ || !this.baseNode_.role) {
      return false;
    }
    return SwitchAccessPredicate.isVisible(this.baseNode_) &&
        super.isValidAndVisible();
  }

  override onFocus(): void {
    super.onFocus();
    this.locationChangedHandler_ = new RepeatedEventHandler(
        this.baseNode_, EventType.LOCATION_CHANGED, () => {
          if (this.isValidAndVisible()) {
            FocusRingManager.setFocusedNode(this);
          } else {
            Navigator.byItem.moveToValidNode();
          }
        }, {exactMatch: true, allAncestors: true});
  }

  override onUnfocus(): void {
    super.onUnfocus();
    if (this.locationChangedHandler_) {
      this.locationChangedHandler_.stop();
    }
  }

  override performAction(action: MenuAction): ActionResponse {
    let ancestor;
    switch (action) {
      case MenuAction.DRILL_DOWN:
        if (this.isGroup()) {
          Navigator.byItem.enterGroup();
          return ActionResponse.CLOSE_MENU;
        }
        // Should not happen.
        console.error('Action DRILL_DOWN received on non-group node.');
        return ActionResponse.NO_ACTION_TAKEN;
      case MenuAction.SELECT:
        this.baseNode_.doDefault();
        return ActionResponse.CLOSE_MENU;
      case MenuAction.SCROLL_DOWN:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollDown(() => {});
        }
        return ActionResponse.RELOAD_MENU;
      case MenuAction.SCROLL_UP:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollUp(() => {});
        }
        return ActionResponse.RELOAD_MENU;
      case MenuAction.SCROLL_RIGHT:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollRight(() => {});
        }
        return ActionResponse.RELOAD_MENU;
      case MenuAction.SCROLL_LEFT:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollLeft(() => {});
        }
        return ActionResponse.RELOAD_MENU;
      default:
        const actions = Object.values(AutomationActionType);
        const automationAction = actions.find((a: string) => a === action);
        if (automationAction) {
          this.baseNode_.performStandardAction(automationAction);
        }
        return ActionResponse.CLOSE_MENU;
    }
  }

  // ================= Private methods =================

  protected getScrollableAncestor_(): AutomationNode {
    let ancestor = this.baseNode_;
    while (!ancestor.scrollable && ancestor.parent) {
      ancestor = ancestor.parent;
    }
    return ancestor;
  }

  // ================= Static methods =================

  static create(
      baseNode: AutomationNode, parent: SARootNode | null): BasicNode {
    const item =
        BasicNode.creators.find(
            (creator: Creator) => creator.predicate(baseNode));
    if (item) {
      return item.creator(baseNode, parent);
    }
    return new BasicNode(baseNode, parent);
  }

  static get creators(): Creator[] {
    return BasicNode.creators_;
  }
}

/**
 * This class handles constructing and traversing a group of onscreen elements
 * based on all the interesting descendants of a single AutomationNode.
 */
export class BasicRootNode extends SARootNode {
  private static builders_: RootBuilder[] = [];

  private childrenChangedHandler_?: RepeatedEventHandler;
  private invalidated_ = false;

  /**
   * WARNING: If you call this constructor, you must *explicitly* set children.
   *     Use the static function BasicRootNode.buildTree for most use cases.
   */
  constructor(baseNode: AutomationNode) {
    super(baseNode);
  }

  // ================= Getters and setters =================

  override get location(): Rect {
    return this.automationNode.location || super.location;
  }

  // ================= General methods =================

  override equals(other: SARootNode | null | undefined): boolean {
    if (!(other instanceof BasicRootNode)) {
      return false;
    }
    return super.equals(other) && this.automationNode === other.automationNode;
  }

  override isEquivalentTo(
      node: AutomationNode | SAChildNode | SARootNode | null): boolean {
    if (node instanceof BasicRootNode || node instanceof BasicNode) {
      return this.automationNode === node.automationNode;
    }

    if (node instanceof SAChildNode) {
      return node.isEquivalentTo(this);
    }
    return this.automationNode === node;
  }

  override isValidGroup(): boolean {
    if (!this.automationNode.role) {
      // If the underlying automation node has been invalidated, return false.
      return false;
    }
    return !this.invalidated_ &&
        SwitchAccessPredicate.isVisible(this.automationNode) &&
        super.isValidGroup();
  }

  override onFocus(): void {
    super.onFocus();
    this.childrenChangedHandler_ = new RepeatedEventHandler(
        this.automationNode, EventType.CHILDREN_CHANGED,
        event => {
          const cache = new SACache();
          if (SwitchAccessPredicate.isInterestingSubtree(event.target, cache)) {
            this.refresh();
          }
        });
  }

  override onUnfocus(): void {
    super.onUnfocus();
    if (this.childrenChangedHandler_) {
      this.childrenChangedHandler_.stop();
    }
  }

  override refreshChildren(): void {
    const childConstructor =
        (node: AutomationNode): BasicNode => BasicNode.create(node, this);
    try {
      BasicRootNode.findAndSetChildren(this, childConstructor);
    } catch (e) {
      this.invalidated_ = true;
    }
  }

  override refresh(): void {
    // Find the currently focused child.
    let focusedChild: SAChildNode | null = null;
    for (const child of this.children) {
      if (child.isFocused()) {
        focusedChild = child;
        break;
      }
    }

    // Update this BasicRootNode's children.
    this.refreshChildren();
    if (this.invalidated_) {
      this.onUnfocus();
      Navigator.byItem.moveToValidNode();
      return;
    }

    // Set the new instance of that child to be the focused node.
    if (focusedChild) {
      for (const child of this.children) {
        if (child.isEquivalentTo(focusedChild)) {
          Navigator.byItem.forceFocusedNode(child);
          return;
        }
      }
    }

    // If we didn't find a match, fall back and reset.
    Navigator.byItem.moveToValidNode();
  }

  // ================= Static methods =================

  static buildTree(rootNode: AutomationNode): BasicRootNode {
    const item = BasicRootNode.builders.find(
        (builder: RootBuilder) => builder.predicate(rootNode));
    if (item) {
      return item.builder(rootNode);
    }

    const root = new BasicRootNode(rootNode);
    const childConstructor =
        (node: AutomationNode): BasicNode => BasicNode.create(node, root);

    BasicRootNode.findAndSetChildren(root, childConstructor);
    return root;
  }

  /**
   * Helper function to connect tree elements, given the root node and a
   * constructor for the child type.
   * @param childConstructor Constructs a child node from an automation node.
   */
  static findAndSetChildren(
      root: BasicRootNode,
      childConstructor: (node: AutomationNode) => SAChildNode): void {
    const interestingChildren = BasicRootNode.getInterestingChildren(root);
    const children = interestingChildren.map(childConstructor)
                         .filter(child => child.isValidAndVisible());

    if (children.length < 1) {
      throw SwitchAccess.error(
          ErrorType.NO_CHILDREN,
          'Root node must have at least 1 interesting child.',
          true /* shouldRecover */);
    }
    children.push(new BackButtonNode(root));
    root.children = children;
  }

  static getInterestingChildren(
      root: BasicRootNode | AutomationNode): AutomationNode[] {
    if (root instanceof BasicRootNode) {
      root = root.automationNode;
    }

    if (root.children.length === 0) {
      return [];
    }
    const interestingChildren: AutomationNode[] = [];
    const treeWalker = new AutomationTreeWalker(
        root, constants.Dir.FORWARD, SwitchAccessPredicate.restrictions(root));
    let node = treeWalker.next().node;

    while (node) {
      interestingChildren.push(node);
      node = treeWalker.next().node;
    }

    return interestingChildren;
  }

  static get builders(): RootBuilder[] {
    return BasicRootNode.builders_;
  }
}

TestImportManager.exportForTesting(BasicNode, BasicRootNode);
