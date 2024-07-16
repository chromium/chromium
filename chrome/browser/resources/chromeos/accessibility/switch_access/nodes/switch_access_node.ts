// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RectUtil} from '/common/rect_util.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {FocusRingManager} from '../focus_ring_manager.js';
import {SwitchAccess} from '../switch_access.js';
import {ActionResponse, ErrorType} from '../switch_access_constants.js';

type AutomationNode = chrome.automation.AutomationNode;
import MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;
type ScreenRect = chrome.accessibilityPrivate.ScreenRect;
type RoleType = chrome.automation.RoleType;

/**
 * This interface represents some object or group of objects on screen
 *     that Switch Access may be interested in interacting with.
 *
 * There is no guarantee of uniqueness; two distinct SAChildNodes may refer
 *     to the same object. However, it is expected that any pair of
 *     SAChildNodes referring to the same interesting object are equal
 *     (calling .equals() returns true).
 */
export abstract class SAChildNode {
  private isFocused_ = false;
  private next_: SAChildNode | null = null;
  private previous_: SAChildNode | null = null;
  private valid_ = true;

  // Abstract methods.

  /** Returns a list of all the actions available for this node. */
  abstract get actions(): MenuAction[];
  /** If this node is a group, returns the analogous SARootNode. */
  abstract asRootNode(): SARootNode | undefined;
  /** The automation node that most closely contains this node. */
  abstract get automationNode(): AutomationNode;
  abstract equals(other: SAChildNode | null | undefined): boolean;
  abstract isEquivalentTo(
      node: AutomationNode | SAChildNode | SARootNode | null): boolean;
  /** Returns whether this node should be displayed as a group. */
  abstract isGroup(): boolean;
  abstract get location(): ScreenRect | undefined;
  /** Performs the specified action on the node, if it is available. */
  abstract performAction(action: MenuAction): ActionResponse;
  abstract get role(): RoleType | undefined;


  // ================= Getters and setters =================

  get group(): SARootNode | null {
    return null;
  }

  set next(newVal: SAChildNode) {
    this.next_ = newVal;
  }

  /** Returns the next node in pre-order traversal. */
  get next(): SAChildNode {
    let next: SAChildNode | null = this;
    while (true) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      next = next!.next_;
      if (!next) {
        this.onInvalidNavigation_(
            ErrorType.NEXT_UNDEFINED,
            'Next node must be set on all SAChildNodes before navigating');
      }
      if (this === next) {
        this.onInvalidNavigation_(ErrorType.NEXT_INVALID, 'No valid next node');
      }
      if (next!.isValidAndVisible()) {
        return next!;
      }
    }
  }

  set previous(newVal: SAChildNode) {
    this.previous_ = newVal;
  }

  /** Returns the previous node in pre-order traversal. */
  get previous(): SAChildNode {
    let previous: SAChildNode | null = this;
    while (true) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      previous = previous!.previous_;
      if (!previous) {
        this.onInvalidNavigation_(
            ErrorType.PREVIOUS_UNDEFINED,
            'Previous node must be set on all SAChildNodes before navigating');
      }
      if (this === previous) {
        this.onInvalidNavigation_(
            ErrorType.PREVIOUS_INVALID, 'No valid previous node');
      }
      if (previous!.isValidAndVisible()) {
        return previous!;
      }
    }
  }

  // ================= General methods =================

  /** Performs the node's default action. */
  doDefaultAction(): void {
    if (!this.isFocused_) {
      return;
    }
    if (this.isGroup()) {
      this.performAction(MenuAction.DRILL_DOWN);
    } else {
      this.performAction(MenuAction.SELECT);
    }
  }

  /** Given a menu action, returns whether it can be performed on this node. */
  hasAction(action: MenuAction): boolean {
    return this.actions.includes(action);
  }

  /** Returns whether the node is currently focused by Switch Access. */
  isFocused(): boolean {
    return this.isFocused_;
  }

  /**
   * Returns whether this node is still both valid and visible onscreen (e.g.
   *    has a location, and, if representing an AutomationNode, not hidden,
   *    not offscreen, not invisible).
   */
  isValidAndVisible(): boolean {
    return this.valid_ && Boolean(this.location);
  }

  /** Called when this node becomes the primary highlighted node. */
  onFocus(): void {
    this.isFocused_ = true;
    FocusRingManager.setFocusedNode(this);
  }

  /** Called when this node stops being the primary highlighted node. */
  onUnfocus(): void {
    this.isFocused_ = false;
  }

  // ================= Debug methods =================

  /** String-ifies the node (for debugging purposes). */
  debugString(
      wholeTree: boolean, prefix: string = '',
      currentNode: SAChildNode | null = null): string {
    if (this.isGroup() && wholeTree) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      return this.asRootNode()!.debugString(
          wholeTree, prefix + '  ', currentNode);
    }

    let str = this.constructor.name + ' role(' + this.role + ') ';

    if (this.automationNode.name) {
      str += 'name(' + this.automationNode.name + ') ';
    }

    const loc = this.location;
    if (loc) {
      str += 'loc(' + RectUtil.toString(loc) + ') ';
    }

    if (this.isGroup()) {
      str += '[isGroup]';
    }

    return str;
  }

  // ================= Private methods =================

  private onInvalidNavigation_(error: ErrorType, message: string): void {
    this.valid_ = false;
    throw SwitchAccess.error(error, message, true /* shouldRecover */);
  }

  /** @return Whether to ignore when computing the SARootNode's location. */
  ignoreWhenComputingUnionOfBoundingBoxes(): boolean {
    return false;
  }
}

/**
 * This class represents the root node of a Switch Access traversal group.
 */
export class SARootNode {
  private children_: SAChildNode[] = [];
  private automationNode_: AutomationNode;

  /**
   * @param autoNode The automation node that most closely contains all of
   * this node's children.
   */
  constructor(autoNode: AutomationNode) {
    this.automationNode_ = autoNode;
  }

  // ================= Getters and setters =================

  /**
   * @return The automation node that most closely contains all of this node's
   * children.
   */
  get automationNode(): AutomationNode {
    return this.automationNode_;
  }

  set children(newVal: SAChildNode[]) {
    this.children_ = newVal;
    this.connectChildren_();
  }

  get children(): SAChildNode[] {
    return this.children_;
  }

  get firstChild(): SAChildNode {
    if (this.children_.length > 0) {
      return this.children_[0];
    } else {
      throw SwitchAccess.error(
          ErrorType.NO_CHILDREN, 'Root nodes must contain children.',
          true /* shouldRecover */);
    }
  }

  get lastChild(): SAChildNode {
    if (this.children_.length > 0) {
      return this.children_[this.children_.length - 1];
    } else {
      throw SwitchAccess.error(
          ErrorType.NO_CHILDREN, 'Root nodes must contain children.',
          true /* shouldRecover */);
    }
  }

  get location(): ScreenRect {
    const children = this.children_.filter(
        c => !c.ignoreWhenComputingUnionOfBoundingBoxes());
    const childLocations = children.map(c => c.location).filter(l => l);
    return RectUtil.unionAll(childLocations as ScreenRect[]);
  }

  // ================= General methods =================

  equals(other: SARootNode): boolean {
    if (!other) {
      return false;
    }
    if (this.children_.length !== other.children_.length) {
      return false;
    }

    let result = true;
    for (let i = 0; i < this.children_.length; i++) {
      if (!this.children_[i]) {
        console.error(
            SwitchAccess.error(ErrorType.NULL_CHILD, 'Child cannot be null.'));
        return false;
      }
      result = result && this.children_[i].equals(other.children_[i]);
    }

    return result;
  }

  /**
   * Looks for and returns the specified node within this node's children.
   * If no equivalent node is found, returns null.
   */
  findChild(
      node: AutomationNode | SAChildNode | SARootNode): SAChildNode | null {
    for (const child of this.children_) {
      if (child.isEquivalentTo(node)) {
        return child;
      }
    }
    return null;
  }

  isEquivalentTo(node: AutomationNode|SARootNode|SAChildNode|null): boolean {
    if (node instanceof SARootNode) {
      return this.equals(node);
    }
    if (node instanceof SAChildNode) {
      return node.isEquivalentTo(this);
    }
    return false;
  }

  isValidGroup(): boolean {
    // Must have one interesting child whose location is important.
    return this.children_
               .filter(
                   (child: SAChildNode) =>
                       !(child.ignoreWhenComputingUnionOfBoundingBoxes()) &&
                       child.isValidAndVisible())
               .length >= 1;
  }

  firstValidChild(): SAChildNode | null {
    const children = this.children_.filter(child => child.isValidAndVisible());
    return children.length > 0 ? children[0] : null;
  }

  /** Called when a group is set as the current group. */
  onFocus(): void {}

  /** Called when a group is no longer the current group. */
  onUnfocus(): void {}

  /** Called when a group is explicitly exited. */
  onExit(): void {}

  /** Called when a group should recalculate its children. */
  refreshChildren(): void {
    this.children =
        this.children.filter((child: SAChildNode) => child.isValidAndVisible());
  }

  /** Called when the group's children may have changed. */
  refresh(): void {}

  // ================= Debug methods =================

  /**
   * String-ifies the node (for debugging purposes).
   * @param wholeTree Whether to recursively descend the tree
   * @param currentNode the currently focused node, to mark.
   */
  debugString(
      wholeTree: boolean = false, prefix: string = '',
      currentNode: SAChildNode | null = null): string {
    let str =
        'Root: ' + this.constructor.name + ' ' + this.automationNode.role + ' ';
    if (this.automationNode.name) {
      str += 'name(' + this.automationNode.name + ') ';
    }

    const loc = this.location;
    if (loc) {
      str += 'loc(' + RectUtil.toString(loc) + ') ';
    }

    for (const child of this.children) {
      str += '\n' + prefix + ((child.equals(currentNode)) ? ' * ' : ' - ');
      str += child.debugString(wholeTree, prefix, currentNode);
    }

    return str;
  }

  // ================= Private methods =================

  /** Helper function to connect children. */
  private connectChildren_(): void {
    if (this.children_.length < 1) {
      console.error(SwitchAccess.error(
          ErrorType.NO_CHILDREN,
          'Root node must have at least 1 interesting child.'));
      return;
    }

    let previous = this.children_[this.children_.length - 1];

    for (let i = 0; i < this.children_.length; i++) {
      const current = this.children_[i];
      previous.next = current;
      current.previous = previous;

      previous = current;
    }
  }
}

export type SANode = SAChildNode|SARootNode;

TestImportManager.exportForTesting(SARootNode);
