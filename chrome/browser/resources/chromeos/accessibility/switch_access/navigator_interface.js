// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SAChildNode, SARootNode} from './nodes/switch_access_node.js';

const AutomationNode = chrome.automation.AutomationNode;

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

  /** Unconditionally exits the current group. @abstract */
  exitGroupUnconditionally() {}

  /**
   * Exits the specified node, if it is the currently focused group.
   * @param {?AutomationNode|!SAChildNode|!SARootNode} node
   * @abstract
   */
  exitIfInGroup(node) {}

  /** @abstract */
  exitKeyboard() {}

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

  /** Jumps into the Switch Access action menu. @abstract */
  jumpToSwitchAccessMenu() {}

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
   * @abstract
   */
  tryMoving(node, getNext, startingNode) {}

  /**
   * Moves to the Switch Access focus up the group stack closest to the ancestor
   * that hasn't been invalidated.
   * @abstract
   */
  moveToValidNode() {}

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
}
