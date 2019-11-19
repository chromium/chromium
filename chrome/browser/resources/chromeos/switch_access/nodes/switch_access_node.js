// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
class SAChildNode {
  constructor() {
    /** @private {?SAChildNode} */
    this.previous_ = null;

    /** @private {?SAChildNode} */
    this.next_ = null;
  }

  // ================= Getters and setters =================

  /**
   * Returns a list of all the actions available for this node.
   * @return {!Array<SAConstants.MenuAction>}
   * @abstract
   */
  get actions() {}

  /**
   * Returns the underlying automation node, if one exists.
   * @return {chrome.automation.AutomationNode}
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
    if (!this.next_) {
      throw SwitchAccess.error(
          SAConstants.ErrorType.NEXT_UNDEFINED,
          'Next node must be set on all SAChildNodes before navigating');
    }
    return this.next_;
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
    if (!this.previous_) {
      throw SwitchAccess.error(
          SAConstants.ErrorType.PREVIOUS_UNDEFINED,
          'Previous node must be set on all SAChildNodes before navigating');
    }
    return this.previous_;
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

  /**
   * @param {SAChildNode} other
   * @return {boolean}
   * @abstract
   */
  equals(other) {}

  /**
   * Given a menu action, returns whether it can be performed on this node.
   * @param {SAConstants.MenuAction} action
   * @return {boolean}
   */
  hasAction(action) {
    return this.actions.includes(action);
  }

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @return {boolean}
   * @abstract
   */
  isEquivalentTo(node) {}

  /**
   * Returns whether this node should be displayed as a group.
   * @return {boolean}
   * @abstract
   */
  isGroup() {}

  /**
   * Called when this node becomes the primary highlighted node.
   */
  onFocus() {}

  /**
   * Called when this node stops being the primary highlighted node.
   */
  onUnfocus() {}

  /**
   * Performs the specified action on the node, if it is available.
   * @param {SAConstants.MenuAction} action
   * @return {boolean} Whether to close the menu. True if the menu should close,
   *     false otherwise.
   * @abstract
   */
  performAction(action) {}

  // ================= Debug methods =================

  /**
   * String-ifies the node (for debugging purposes).
   * @param {boolean} wholeTree Whether to recursively include descendants.
   * @param {string=} prefix
   * @param {SAChildNode=} currentNode the currentNode, to highlight.
   * @return {string}
   */
  debugString(wholeTree, prefix = '', currentNode = null) {
    if (this.isGroup() && wholeTree) {
      return this.asRootNode().debugString(
          wholeTree, prefix + '  ', currentNode);
    }

    let str = this.role + ' ';

    const autoNode = this.automationNode;
    if (autoNode && autoNode.name) {
      str += 'name(' + autoNode.name + ') ';
    }

    const loc = this.location;
    if (loc) {
      str += 'loc(' + RectHelper.toString(loc) + ') ';
    }

    if (this.isGroup()) {
      str += '[isGroup]';
    }

    return str;
  }
}

/**
 * This class represents the root node of a Switch Access traversal group.
 */
class SARootNode {
  constructor() {
    /** @private {!Array<!SAChildNode>} */
    this.children_ = [];
  }

  // ================= Getters and setters =================

  /** @return {chrome.automation.AutomationNode} */
  get automationNode() {}

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
          'Root nodes must contain children.');
    }
  }

  /** @return {!SAChildNode} */
  get lastChild() {
    if (this.children_.length > 0) {
      return this.children_[this.children_.length - 1];
    } else {
      throw SwitchAccess.error(
          SAConstants.ErrorType.NO_CHILDREN,
          'Root nodes must contain children.');
    }
  }

  /** @return {!chrome.accessibilityPrivate.ScreenRect} */
  get location() {
    let children = this.children_.filter((c) => !(c instanceof BackButtonNode));
    let childLocations = children.map((c) => c.location);
    return RectHelper.unionAll(childLocations);
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
        throw SwitchAccess.error(
            SAConstants.ErrorType.NULL_CHILD, 'Child cannot be null.');
      }
      result = result && this.children_[i].equals(other.children_[i]);
    }

    return result;
  }

  /**
   * @param {chrome.automation.AutomationNode} automationNode
   * @return {boolean}
   */
  isEquivalentTo(automationNode) {
    return false;
  }

  /** @return {boolean} */
  isValid() {
    return true;
  }

  /** Called when a group is exiting. */
  onExit() {}

  // ================= Debug methods =================

  /**
   * String-ifies the node (for debugging purposes).
   * @param {boolean=} wholeTree Whether to recursively descend the tree
   * @param {string=} prefix
   * @param {SAChildNode} currentNode the currently focused node, to mark.
   * @return {string}
   */
  debugString(wholeTree = false, prefix = '', currentNode = null) {
    const autoNode = this.automationNode;
    let str = 'Root: ';
    if (autoNode && autoNode.role) {
      str += autoNode.role + ' ';
    }
    if (autoNode && autoNode.name) {
      str += 'name(' + autoNode.name + ') ';
    }

    const loc = this.location;
    if (loc) {
      str += 'loc(' + RectHelper.toString(loc) + ') ';
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
      throw SwitchAccess.error(
          SAConstants.ErrorType.NO_CHILDREN,
          'Root node must have at least 1 interesting child.');
    }

    let previous = this.children_[this.children_.length - 1];

    for (let i = 0; i < this.children_.length; i++) {
      let current = this.children_[i];
      previous.next = current;
      current.previous = previous;

      previous = current;
    }
  }
}
