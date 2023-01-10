// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {constants} from '../common/constants.js';

import {SAChildNode, SANode, SARootNode} from './nodes/switch_access_node.js';

const AutomationNode = chrome.automation.AutomationNode;
const MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;

/** @abstract */
export class ItemNavigatorInterface {
  /**
   * @param {!SAChildNode} node
   * @return {boolean}
   * @abstract
   */
  currentGroupHasChild(node) {}

  /**
   * Enters |this.node_|.
   * @abstract
   */
  enterGroup() {}

  /**
   * Puts focus on the virtual keyboard, if the current node is a text input.
   * @abstract
   */
  enterKeyboard() {}

  /**
   * Unconditionally exits the current group.
   * @abstract
   */
  exitGroupUnconditionally() {}

  /**
   * Exits the specified node, if it is the currently focused group.
   * @param {?AutomationNode|SANode} node
   * @abstract
   */
  exitIfInGroup(node) {}

  /**
   * @return {!Promise}
   * @abstract
   */
  async exitKeyboard() {}

  /**
   * Forces the current node to be |node|.
   * Should only be called by subclasses of SARootNode and
   *    only when they are focused.
   * @param {!SAChildNode} node
   * @abstract
   */
  forceFocusedNode(node) {}

  /**
   * Returns the current Switch Access tree, for debugging purposes.
   * @param {boolean} wholeTree Whether to print the whole tree, or just the
   * current focus.
   * @return {!SARootNode}
   * @abstract
   */
  getTreeForDebugging(wholeTree) {}

  /**
   * Jumps to a specific automation node. Maintains the history when navigating.
   * @param {AutomationNode} automationNode
   * @abstract
   */
  jumpTo(automationNode) {}

  /**
   * Move to the previous interesting node.
   * @abstract
   */
  moveBackward() {}

  /**
   * Move to the next interesting node.
   * @abstract
   */
  moveForward() {}

  /**
   * Tries to move to another node, |node|, but if |node| is a window that's not
   * in the foreground it will use |getNext| to find the next node to try.
   * Checks against |startingNode| to ensure we don't get stuck in an infinite
   * loop.
   * @param {!SAChildNode} node The node to try to move into.
   * @param {function(!SAChildNode): !SAChildNode} getNext gets the next node to
   *     try if we cannot move to |next|. Takes |next| as a parameter.
   * @param {!SAChildNode} startingNode The first node in the sequence. If we
   *     loop back to this node, stop trying to move, as there are no other
   *     nodes we can move to.
   * @return {!Promise}
   * @abstract
   */
  async tryMoving(node, getNext, startingNode) {}

  /**
   * Moves to the Switch Access focus up the group stack closest to the ancestor
   * that hasn't been invalidated.
   * @abstract
   */
  moveToValidNode() {}

  /**
   * Restarts item scanning from the last point chosen by point scanning.
   * @abstract
   */
  restart() {}

  /**
   * Restores the suspended group and focus, if there is one.
   * @abstract
   */
  restoreSuspendedGroup() {}

  /**
   * Saves the current focus and group, and then exits the group.
   * @abstract
   */
  suspendCurrentGroup() {}

  // =============== Getter Methods ==============

  /**
   * Returns the currently focused node.
   * @return {!SAChildNode}
   * @abstract
   */
  get currentNode() {}

  /**
   * Returns the desktop automation node object.
   * @return {!AutomationNode}
   * @abstract
   */
  get desktopNode() {}
}

/** @abstract */
export class PointNavigatorInterface {
  /**
   * Returns the current point scan point.
   * @return {!constants.Point}
   */
  get currentPoint() {}

  /** Starts point scanning. */
  start() {}

  /** Stops point scanning. */
  stop() {}

  /**
   * Performs a mouse action at the currentPoint().
   * @param {MenuAction} action
   */
  performMouseAction(action) {}
}
