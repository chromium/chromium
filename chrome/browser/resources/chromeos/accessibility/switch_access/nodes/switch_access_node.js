// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RectUtil} from '../../common/rect_util.js';
import {FocusRingManager} from '../focus_ring_manager.js';
import {SwitchAccess} from '../switch_access.js';
import {SAConstants, SwitchAccessMenuAction} from '../switch_access_constants.js';

const AutomationNode = chrome.automation.AutomationNode;

/**
 * This interface represents some object or group of objects on screen
 *     that Switch Access may be interested in interacting with.
 *
 * There is no guarantee of uniqueness; two distinct SAChildNodes may refer
 *     to the same object. However, it is expected that any pair of
 *     SAChildNodes referring to the same interesting object are equal
 *     (calling .equals() returns true).
 * @abstract
 */
export class SAChildNode {
  constructor() {
    /** @private {boolean} */
    this.isFocused_ = false;

    /** @private {?SAChildNode} */
    this.next_ = null;

    /** @private {?SAChildNode} */
    this.previous_ = null;

    /** @private {boolean} */
    this.valid_ = true;
  }

  // ================= Getters and setters =================

  /**
   * Returns a list of all the actions available for this node.
   * @return {!Array<SwitchAccessMenuAction>}
   * @abstract
   */
  get actions() {}

  /**
   * The automation node that most closely contains this node.
   * @return {!AutomationNode}
   * @abstract
   */
  get automationNode() {}

  /**
   * @return {chrome.accessibilityPrivate.ScreenRect|undefined}
   * @abstract
   */
  get location() {}

  /** @param {!SAChildNode} newVal */
  set next(newVal) {
    this.next_ = newVal;
  }

  /**
   * Returns the next node in pre-order traversal.
   * @return {!SAChildNode}
   */
  get next() {
    let next = this;
    while (true) {
      next = next.next_;
      if (!next) {
        this.onInvalidNavigation_(
            SAConstants.ErrorType.NEXT_UNDEFINED,
            'Next node must be set on all SAChildNodes before navigating');
      }
      if (this === next) {
        this.onInvalidNavigation_(
            SAConstants.ErrorType.NEXT_INVALID, 'No valid next node');
      }
      if (next.isValidAndVisible()) {
        return next;
      }
    }
  }

  /** @param {!SAChildNode} newVal */
  set previous(newVal) {
    this.previous_ = newVal;
  }

  /**
   * Returns the previous node in pre-order traversal.
   * @return {!SAChildNode}
   */
  get previous() {
    let previous = this;
    while (true) {
      previous = previous.previous_;
      if (!previous) {
        this.onInvalidNavigation_(
            SAConstants.ErrorType.PREVIOUS_UNDEFINED,
            'Previous node must be set on all SAChildNodes before navigating');
      }
      if (this === previous) {
        this.onInvalidNavigation_(
            SAConstants.ErrorType.PREVIOUS_INVALID, 'No valid previous node');
      }
      if (previous.isValidAndVisible()) {
        return previous;
      }
    }
  }

  /**
   * @return {chrome.automation.RoleType|undefined}
   * @abstract
   */
  get role() {}

  // ================= General methods =================

  /**
   * If this node is a group, returns the analogous SARootNode.
   * @return {SARootNode}
   * @abstract
   */
  asRootNode() {}

  /** Performs the node's default action. */
  doDefaultAction() {
    if (!this.isFocused_) {
      return;
    }
    this.performAction(SwitchAccessMenuAction.SELECT);
  }

  /**
   * @param {SAChildNode} other
   * @return {boolean}
   * @abstract
   */
  equals(other) {}

  /**
   * Given a menu action, returns whether it can be performed on this node.
   * @param {SwitchAccessMenuAction} action
   * @return {boolean}
   */
  hasAction(action) {
    return this.actions.includes(action);
  }

  /**
   * @param {?AutomationNode|!SAChildNode|!SARootNode} node
   * @return {boolean}
   * @abstract
   */
  isEquivalentTo(node) {}

  /**
   * Returns whether the node is currently focused by Switch Access
   * @return {boolean}
   */
  isFocused() {
    return this.isFocused_;
  }

  /**
   * Returns whether this node should be displayed as a group.
   * @return {boolean}
   * @abstract
   */
  isGroup() {}

  /**
   * Returns whether this node is still both valid and visible onscreen (e.g.
   *    has a location, and, if representing an AutomationNode, not hidden,
   *    not offscreen, not invisible).
   * @return {boolean}
   */
  isValidAndVisible() {
    return this.valid_ && Boolean(this.location);
  }

  /**
   * Called when this node becomes the primary highlighted node.
   */
  onFocus() {
    this.isFocused_ = true;
    FocusRingManager.setFocusedNode(this);
  }

  /**
   * Called when this node stops being the primary highlighted node.
   */
  onUnfocus() {
    this.isFocused_ = false;
  }

  /**
   * Performs the specified action on the node, if it is available.
   * @param {SwitchAccessMenuAction} action
   * @return {SAConstants.ActionResponse} What action the menu should perform in
   *      response.
   * @abstract
   */
  performAction(action) {}

  // ================= Debug methods =================

  /**
   * String-ifies the node (for debugging purposes).
   * @param {boolean} wholeTree Whether to recursively include descendants.
   * @param {string} prefix
   * @param {?SAChildNode} currentNode the currentNode, to highlight.
   * @return {string}
   */
  debugString(wholeTree, prefix = '', currentNode = null) {
    if (this.isGroup() && wholeTree) {
      return this.asRootNode().debugString(
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

  /**
   *
   * @param {SAConstants.ErrorType} error
   * @param {string} message
   */
  onInvalidNavigation_(error, message) {
    this.valid_ = false;
    throw SwitchAccess.error(error, message, true /* shouldRecover */);
  }

  /**
   * @return {boolean} Whether to ignore when computing the SARootNode's
   *     location.
   */
  ignoreWhenComputingUnionOfBoundingBoxes() {
    return false;
  }


  /** @return {SARootNode} */
  get group() {
    return null;
  }
}

/**
 * This class represents the root node of a Switch Access traversal group.
 */
export class SARootNode {
  /**
   * @param {!AutomationNode} autoNode The automation node that most closely
   *     contains all of this node's children.
   */
  constructor(autoNode) {
    /** @private {!Array<!SAChildNode>} */
    this.children_ = [];

    /** @private {!AutomationNode} */
    this.automationNode_ = autoNode;
  }

  // ================= Getters and setters =================

  /**
   * @return {!AutomationNode} The automation node that most closely
   *     contains all of this node's children.
   */
  get automationNode() {
    return this.automationNode_;
  }

  /** @param {!Array<!SAChildNode>} newVal */
  set children(newVal) {
    this.children_ = newVal;
    this.connectChildren_();
  }

  /** @return {!Array<!SAChildNode>} */
  get children() {
    return this.children_;
  }

  /** @return {!SAChildNode} */
  get firstChild() {
    if (this.children_.length > 0) {
      return this.children_[0];
    } else {
      throw SwitchAccess.error(
          SAConstants.ErrorType.NO_CHILDREN,
          'Root nodes must contain children.', true /* shouldRecover */);
    }
  }

  /** @return {!SAChildNode} */
  get lastChild() {
    if (this.children_.length > 0) {
      return this.children_[this.children_.length - 1];
    } else {
      throw SwitchAccess.error(
          SAConstants.ErrorType.NO_CHILDREN,
          'Root nodes must contain children.', true /* shouldRecover */);
    }
  }

  /** @return {!chrome.accessibilityPrivate.ScreenRect} */
  get location() {
    const children = this.children_.filter(
        c => !c.ignoreWhenComputingUnionOfBoundingBoxes());
    const childLocations = children.map(c => c.location);
    return RectUtil.unionAll(childLocations);
  }

  // ================= General methods =================

  /**
   * @param {SARootNode} other
   * @return {boolean}
   */
  equals(other) {
    if (!other) {
      return false;
    }
    if (this.children_.length !== other.children_.length) {
      return false;
    }

    let result = true;
    for (let i = 0; i < this.children_.length; i++) {
      if (!this.children_[i]) {
        console.error(SwitchAccess.error(
            SAConstants.ErrorType.NULL_CHILD, 'Child cannot be null.'));
        return false;
      }
      result = result && this.children_[i].equals(other.children_[i]);
    }

    return result;
  }

  /**
   * Looks for and returns the specified node within this node's children.
   * If no equivalent node is found, returns null.
   * @param {?AutomationNode|!SAChildNode|!SARootNode} node
   * @return {?SAChildNode}
   */
  findChild(node) {
    for (const child of this.children_) {
      if (child.isEquivalentTo(node)) {
        return child;
      }
    }
    return null;
  }

  /**
   * @param {?AutomationNode|!SARootNode|!SAChildNode} node
   * @return {boolean}
   */
  isEquivalentTo(node) {
    if (node instanceof SARootNode) {
      return this.equals(node);
    }
    if (node instanceof SAChildNode) {
      return node.isEquivalentTo(this);
    }
    return false;
  }

  /** @return {boolean} */
  isValidGroup() {
    // Must have one interesting child whose location is important.
    return this.children_
               .filter(
                   child =>
                       !(child.ignoreWhenComputingUnionOfBoundingBoxes()) &&
                       child.isValidAndVisible())
               .length >= 1;
  }

  /** @return {SAChildNode} */
  firstValidChild() {
    const children = this.children_.filter(child => child.isValidAndVisible());
    return children.length > 0 ? children[0] : null;
  }

  /** Called when a group is set as the current group. */
  onFocus() {}

  /** Called when a group is no longer the current group. */
  onUnfocus() {}

  /** Called when a group is explicitly exited. */
  onExit() {}

  /** Called when a group should recalculate its children. */
  refreshChildren() {
    this.children = this.children.filter(child => child.isValidAndVisible());
  }

  /** Called when the group's children may have changed. */
  refresh() {}

  // ================= Debug methods =================

  /**
   * String-ifies the node (for debugging purposes).
   * @param {boolean} wholeTree Whether to recursively descend the tree
   * @param {string} prefix
   * @param {?SAChildNode} currentNode the currently focused node, to mark.
   * @return {string}
   */
  debugString(wholeTree = false, prefix = '', currentNode = null) {
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

  /**
   * Helper function to connect children.
   * @private
   */
  connectChildren_() {
    if (this.children_.length < 1) {
      console.error(SwitchAccess.error(
          SAConstants.ErrorType.NO_CHILDREN,
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

/** @typedef {!SAChildNode|!SARootNode} */
export let SANode;
