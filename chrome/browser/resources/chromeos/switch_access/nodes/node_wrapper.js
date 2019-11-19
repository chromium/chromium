// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const AutomationNode = chrome.automation.AutomationNode;

/**
 * This class handles interactions with an onscreen element based on a single
 * AutomationNode.
 */
class NodeWrapper extends SAChildNode {
  /**
   * @param {!AutomationNode} baseNode
   * @param {?SARootNode} parent
   */
  constructor(baseNode, parent) {
    super();
    /** @private {!AutomationNode} */
    this.baseNode_ = baseNode;

    /** @private {boolean} */
    this.isGroup_ = SwitchAccessPredicate.isGroup(this.baseNode_, parent);
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    let actions = [];
    if (SwitchAccessPredicate.isTextInput(this.baseNode_)) {
      actions.push(SAConstants.MenuAction.OPEN_KEYBOARD);
      actions.push(SAConstants.MenuAction.DICTATION);
    } else {
      actions.push(SAConstants.MenuAction.SELECT);
    }

    const ancestor = this.getScrollableAncestor_();
    if (ancestor.scrollable) {
      if (ancestor.scrollX > ancestor.scrollXMin) {
        actions.push(SAConstants.MenuAction.SCROLL_LEFT);
      }
      if (ancestor.scrollX < ancestor.scrollXMax) {
        actions.push(SAConstants.MenuAction.SCROLL_RIGHT);
      }
      if (ancestor.scrollY > ancestor.scrollYMin) {
        actions.push(SAConstants.MenuAction.SCROLL_UP);
      }
      if (ancestor.scrollY < ancestor.scrollYMax) {
        actions.push(SAConstants.MenuAction.SCROLL_DOWN);
      }
    }
    const standardActions = /** @type {!Array<!SAConstants.MenuAction>} */ (
        this.baseNode_.standardActions.filter(
            action => Object.values(SAConstants.MenuAction).includes(action)));

    return actions.concat(standardActions);
  }

  /** @override */
  get automationNode() {
    return this.baseNode_;
  }

  /** @override */
  get location() {
    return this.baseNode_.location;
  }

  /** @override */
  get role() {
    return this.baseNode_.role;
  }

  // ================= General methods =================

  /** @override */
  asRootNode() {
    if (!this.isGroup()) {
      return null;
    }
    return RootNodeWrapper.buildTree(this.baseNode_);
  }

  /** @override */
  equals(other) {
    if (!other || !(other instanceof NodeWrapper)) {
      return false;
    }

    other = /** @type {!NodeWrapper} */ (other);
    return other.baseNode_ === this.baseNode_;
  }

  /** @override */
  isEquivalentTo(node) {
    return this.baseNode_ === node;
  }

  /** @override */
  isGroup() {
    return this.isGroup_;
  }

  /** @override */
  performAction(action) {
    let ancestor;
    switch (action) {
      case SAConstants.MenuAction.OPEN_KEYBOARD:
        this.baseNode_.focus();
        return true;
      case SAConstants.MenuAction.SELECT:
        this.baseNode_.doDefault();
        return true;
      case SAConstants.MenuAction.DICTATION:
        chrome.accessibilityPrivate.toggleDictation();
        return true;
      case SAConstants.MenuAction.SCROLL_DOWN:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollDown(() => {});
        }
        return true;
      case SAConstants.MenuAction.SCROLL_UP:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollUp(() => {});
        }
        return true;
      case SAConstants.MenuAction.SCROLL_RIGHT:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollRight(() => {});
        }
        return true;
      case SAConstants.MenuAction.SCROLL_LEFT:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollLeft(() => {});
        }
        return true;
      default:
        if (Object.values(chrome.automation.ActionType).includes(action)) {
          this.baseNode_.performStandardAction(
              /** @type {chrome.automation.ActionType} */ (action));
        }
        return true;
    }
  }

  // ================= Private methods =================

  /**
   * @return {AutomationNode}
   * @protected
   */
  getScrollableAncestor_() {
    let ancestor = this.baseNode_;
    while (!ancestor.scrollable && ancestor.parent)
      ancestor = ancestor.parent;
    return ancestor;
  }
}

/**
 * This class handles constructing and traversing a group of onscreen elements
 * based on all the interesting descendants of a single AutomationNode.
 */
class RootNodeWrapper extends SARootNode {
  /**
   * @param {!AutomationNode} baseNode
   */
  constructor(baseNode) {
    super();

    /** @private {!AutomationNode} */
    this.baseNode_ = baseNode;
  }

  // ================= Getters and setters =================

  /** @override */
  get automationNode() {
    return this.baseNode_;
  }

  /** @override */
  get location() {
    return this.baseNode_.location || super.location;
  }

  // ================= General methods =================

  /** @override */
  equals(other) {
    if (!(other instanceof RootNodeWrapper)) {
      return false;
    }

    other = /** @type {!RootNodeWrapper} */ (other);
    return super.equals(other) && this.baseNode_ === other.baseNode_;
  }

  /** @override */
  isEquivalentTo(automationNode) {
    return this.baseNode_ === automationNode;
  }

  /** @override */
  isValid() {
    return !!this.baseNode_.role;
  }

  // ================= Static methods =================

  /**
   * @param {!AutomationNode} desktop
   * @return {!RootNodeWrapper}
   */
  static buildDesktopTree(desktop) {
    const root = new RootNodeWrapper(desktop);
    const interestingChildren = RootNodeWrapper.getInterestingChildren(root);

    if (interestingChildren.length < 1) {
      throw SwitchAccess.error(
          SAConstants.ErrorType.MALFORMED_DESKTOP,
          'Desktop node must have at least 1 interesting child.');
    }

    const childConstructor = (autoNode) => new NodeWrapper(autoNode, root);
    let children = interestingChildren.map(childConstructor);
    root.children = children;

    return root;
  }

  /**
   * @param {!AutomationNode} rootNode
   * @return {!RootNodeWrapper}
   */
  static buildTree(rootNode) {
    const root = new RootNodeWrapper(rootNode);
    const childConstructor = (node) => new NodeWrapper(node, root);

    RootNodeWrapper.findAndSetChildren(root, childConstructor);
    return root;
  }

  /**
   * Helper function to connect tree elements, given the root node and a
   * constructor for the child type.
   * @param {!RootNodeWrapper} root
   * @param {function(!AutomationNode): !SAChildNode} childConstructor
   *     Constructs a child node from an automation node.
   */
  static findAndSetChildren(root, childConstructor) {
    const interestingChildren = RootNodeWrapper.getInterestingChildren(root);

    if (interestingChildren.length < 1) {
      throw SwitchAccess.error(
          SAConstants.ErrorType.NO_CHILDREN,
          'Root node must have at least 1 interesting child.');
    }
    let children = interestingChildren.map(childConstructor);
    children.push(new BackButtonNode(root));
    root.children = children;
  }

  /**
   * @param {!RootNodeWrapper} root
   * @return {!Array<!AutomationNode>}
   */
  static getInterestingChildren(root) {
    let interestingChildren = [];
    let treeWalker = new AutomationTreeWalker(
        root.baseNode_, constants.Dir.FORWARD,
        SwitchAccessPredicate.restrictions(root));
    let node = treeWalker.next().node;

    while (node) {
      interestingChildren.push(node);
      node = treeWalker.next().node;
    }

    return interestingChildren;
  }
}
