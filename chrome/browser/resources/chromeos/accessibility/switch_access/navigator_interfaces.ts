// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {constants} from '/common/constants.js';

import {SAChildNode, SANode, SARootNode} from './nodes/switch_access_node.js';

import MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;
type AutomationNode = chrome.automation.AutomationNode;
type Point = constants.Point;

export abstract class ItemNavigatorInterface {
  abstract currentGroupHasChild(node: SAChildNode): boolean;

  /** Enters |this.node_|. */
  abstract enterGroup(): void;

  /**
   * Puts focus on the virtual keyboard, if the current node is a text input.
   */
  abstract enterKeyboard(): void;

  /** Unconditionally exits the current group. */
  abstract exitGroupUnconditionally(): void;

  /** Exits the specified node, if it is the currently focused group. */
  abstract exitIfInGroup(node: (AutomationNode|SANode|null)): void;

  abstract exitKeyboard(): Promise<void>;

  /**
   * Forces the current node to be |node|.
   * Should only be called by subclasses of SARootNode and
   *    only when they are focused.
   */
  abstract forceFocusedNode(node: SAChildNode): void;

  /**
   * Returns the current Switch Access tree, for debugging purposes.
   * @param wholeTree Whether to print the whole tree, or just the
   * current focus.
   */
  abstract getTreeForDebugging(wholeTree: boolean): SARootNode;

  /**
   * Jumps to a specific automation node. Maintains the history when
   * navigating.
   */
  abstract jumpTo(automationNode: AutomationNode): void;

  /** Move to the previous interesting node. */
  abstract moveBackward(): void;

  /** Move to the next interesting node. */
  abstract moveForward(): void;

  /**
   * Tries to move to another node, |node|, but if |node| is a window that's not
   * in the foreground it will use |getNext| to find the next node to try.
   * Checks against |startingNode| to ensure we don't get stuck in an infinite
   * loop.
   * @param node The node to try to move into.
   * @param getNext gets the next node to
   *     try if we cannot move to |next|. Takes |next| as a parameter.
   * @param startingNode The first node in the sequence. If we
   *     loop back to this node, stop trying to move, as there are no other
   *     nodes we can move to.
   */
  abstract tryMoving(
      _node: SAChildNode, _getNext: (node: SAChildNode) => SAChildNode,
      _startingNode: SAChildNode): Promise<void>;

  /**
   * Moves to the Switch Access focus up the group stack closest to the ancestor
   * that hasn't been invalidated.
   */
  abstract moveToValidNode(): void;

  /** Restarts item scanning from the last point chosen by point scanning. */
  abstract restart(): void;

  /** Restores the suspended group and focus, if there is one. */
  abstract restoreSuspendedGroup(): void;

  /** Saves the current focus and group, and then exits the group. */
  abstract suspendCurrentGroup(): void;

  /**
   * Called when everything has been initialized to add the listeners and find
   * the initial focus.
   */
   abstract start(): void;

  // =============== Getter Methods ==============

  /** Returns the currently focused node. */
  abstract get currentNode(): SAChildNode;

  /** Returns the desktop automation node object. */
  abstract get desktopNode(): AutomationNode;
}

export abstract class PointNavigatorInterface {
  /** Returns the current point scan point. */
  abstract get currentPoint(): Point;

  /** Starts point scanning. */
  abstract start(): void;

  /** Stops point scanning. */
  abstract stop(): void;

  /** Performs a mouse action at the currentPoint(). */
  abstract performMouseAction(action: MenuAction): void;
}
