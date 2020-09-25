// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles interactions with an onscreen element based on a single
 * AutomationNode.
 */
class BasicNode extends SAChildNode {
  /**
   * @param {!AutomationNode} baseNode
   * @param {?SARootNode} parent
   * @protected
   */
  constructor(baseNode, parent) {
    super();
    /** @private {!AutomationNode} */
    this.baseNode_ = baseNode;

    /** @private {?SARootNode} */
    this.parent_ = parent;

    /** @private {RepeatedEventHandler} */
    this.locationChangedHandler_;
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    const actions = [];
    actions.push(SwitchAccessMenuAction.SELECT);

    const ancestor = this.getScrollableAncestor_();
    if (ancestor.scrollable) {
      if (ancestor.scrollX > ancestor.scrollXMin) {
        actions.push(SwitchAccessMenuAction.SCROLL_LEFT);
      }
      if (ancestor.scrollX < ancestor.scrollXMax) {
        actions.push(SwitchAccessMenuAction.SCROLL_RIGHT);
      }
      if (ancestor.scrollY > ancestor.scrollYMin) {
        actions.push(SwitchAccessMenuAction.SCROLL_UP);
      }
      if (ancestor.scrollY < ancestor.scrollYMax) {
        actions.push(SwitchAccessMenuAction.SCROLL_DOWN);
      }
    }
    const standardActions = /** @type {!Array<!SwitchAccessMenuAction>} */ (
        this.baseNode_.standardActions.filter(
            action => Object.values(SwitchAccessMenuAction).includes(action)));

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
    return BasicRootNode.buildTree(this.baseNode_);
  }

  /** @override */
  equals(other) {
    if (!other || !(other instanceof BasicNode)) {
      return false;
    }

    other = /** @type {!BasicNode} */ (other);
    return other.baseNode_ === this.baseNode_;
  }

  /** @override */
  isEquivalentTo(node) {
    if (node instanceof BasicNode || node instanceof BasicRootNode) {
      return this.baseNode_ === node.baseNode_;
    }

    if (node instanceof SAChildNode) {
      return node.isEquivalentTo(this);
    }
    return this.baseNode_ === node;
  }

  /** @override */
  isGroup() {
    const cache = new SACache();
    return SwitchAccessPredicate.isGroup(this.baseNode_, this.parent_, cache);
  }

  /** @override */
  isValidAndVisible() {
    // Nodes may have been deleted or orphaned.
    if (!this.baseNode_ || !this.baseNode_.role) {
      return false;
    }
    return SwitchAccessPredicate.isVisible(this.baseNode_) &&
        super.isValidAndVisible();
  }

  /** @override */
  onFocus() {
    super.onFocus();
    this.locationChangedHandler_ = new RepeatedEventHandler(
        this.baseNode_, chrome.automation.EventType.LOCATION_CHANGED, () => {
          if (this.isValidAndVisible()) {
            FocusRingManager.setFocusedNode(this);
          } else {
            NavigationManager.moveToValidNode();
          }
        }, {exactMatch: true, allAncestors: true});
  }

  /** @override */
  onUnfocus() {
    super.onUnfocus();
    if (this.locationChangedHandler_) {
      this.locationChangedHandler_.stop();
    }
  }

  /** @override */
  performAction(action) {
    let ancestor;
    switch (action) {
      case SwitchAccessMenuAction.SELECT:
        if (this.isGroup()) {
          NavigationManager.enterGroup();
        } else {
          this.baseNode_.doDefault();
        }
        return SAConstants.ActionResponse.CLOSE_MENU;
      case SwitchAccessMenuAction.SCROLL_DOWN:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollDown();
        }
        return SAConstants.ActionResponse.RELOAD_MAIN_MENU;
      case SwitchAccessMenuAction.SCROLL_UP:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollUp();
        }
        return SAConstants.ActionResponse.RELOAD_MAIN_MENU;
      case SwitchAccessMenuAction.SCROLL_RIGHT:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollRight();
        }
        return SAConstants.ActionResponse.RELOAD_MAIN_MENU;
      case SwitchAccessMenuAction.SCROLL_LEFT:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollLeft();
        }
        return SAConstants.ActionResponse.RELOAD_MAIN_MENU;
      default:
        if (Object.values(chrome.automation.ActionType).includes(action)) {
          this.baseNode_.performStandardAction(
              /** @type {chrome.automation.ActionType} */ (action));
        }
        return SAConstants.ActionResponse.CLOSE_MENU;
    }
  }

  // ================= Private methods =================

  /**
   * @return {AutomationNode}
   * @protected
   */
  getScrollableAncestor_() {
    let ancestor = this.baseNode_;
    while (!ancestor.scrollable && ancestor.parent) {
      ancestor = ancestor.parent;
    }
    return ancestor;
  }

  // ================= Static methods =================

  /**
   * @param {!AutomationNode} baseNode
   * @param {?SARootNode} parent
   * @return {!BasicNode}
   */
  static create(baseNode, parent) {
    if (SwitchAccessPredicate.isTextInput(baseNode)) {
      return new EditableTextNode(baseNode, parent);
    }

    if (AutomationPredicate.comboBox(baseNode)) {
      return new ComboBoxNode(baseNode, parent);
    }
    switch (baseNode.role) {
      case chrome.automation.RoleType.SLIDER:
        return new SliderNode(baseNode, parent);
      case chrome.automation.RoleType.TAB:
        if (baseNode.root.role === RoleType.DESKTOP) {
          return TabNode.create(baseNode, parent);
        }
        // else, fall through to default case
      default:
        return new BasicNode(baseNode, parent);
    }
  }
}

/**
 * This class handles constructing and traversing a group of onscreen elements
 * based on all the interesting descendants of a single AutomationNode.
 */
class BasicRootNode extends SARootNode {
  /**
   * WARNING: If you call this constructor, you must *explicitly* set children.
   *     Use the static function BasicRootNode.buildTree for most use cases.
   * @param {!AutomationNode} baseNode
   */
  constructor(baseNode) {
    super(baseNode);

    /** @private {boolean} */
    this.invalidated_ = false;

    /** @private {RepeatedEventHandler} */
    this.childrenChangedHandler_;
  }

  // ================= Getters and setters =================

  /** @override */
  get location() {
    return this.automationNode.location || super.location;
  }

  // ================= General methods =================

  /** @override */
  equals(other) {
    if (!(other instanceof BasicRootNode)) {
      return false;
    }
    return super.equals(other) && this.automationNode === other.automationNode;
  }

  /** @override */
  isEquivalentTo(node) {
    if (node instanceof BasicRootNode || node instanceof BasicNode) {
      return this.automationNode === node.automationNode;
    }

    if (node instanceof SAChildNode) {
      return node.isEquivalentTo(this);
    }
    return this.automationNode === node;
  }

  /** @override */
  isValidGroup() {
    if (!this.automationNode.role) {
      // If the underlying automation node has been invalidated, return false.
      return false;
    }
    return !this.invalidated_ &&
        SwitchAccessPredicate.isVisible(this.automationNode) &&
        super.isValidGroup();
  }

  /** @override */
  onFocus() {
    super.onFocus();
    this.childrenChangedHandler_ = new RepeatedEventHandler(
        this.automationNode, chrome.automation.EventType.CHILDREN_CHANGED,
        this.refresh.bind(this));
  }

  /** @override */
  onUnfocus() {
    super.onUnfocus();
    if (this.childrenChangedHandler_) {
      this.childrenChangedHandler_.stop();
    }
  }

  /** @override */
  refreshChildren() {
    const childConstructor = (node) => BasicNode.create(node, this);
    try {
      BasicRootNode.findAndSetChildren(this, childConstructor);
    } catch (e) {
      this.invalidated_ = true;
    }
  }

  /** @override */
  refresh() {
    // Find the currently focused child.
    let focusedChild = null;
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
      NavigationManager.moveToValidNode();
      return;
    }

    // Set the new instance of that child to be the focused node.
    if (focusedChild) {
      for (const child of this.children) {
        if (child.isEquivalentTo(focusedChild)) {
          NavigationManager.forceFocusedNode(child);
          return;
        }
      }
    }

    // If we didn't find a match, fall back and reset.
    NavigationManager.moveToValidNode();
  }

  // ================= Static methods =================

  /**
   * @param {!AutomationNode} rootNode
   * @return {!BasicRootNode}
   */
  static buildTree(rootNode) {
    if (rootNode.role === chrome.automation.RoleType.KEYBOARD) {
      return KeyboardRootNode.buildTree();
    }
    if (SwitchAccessPredicate.isWindow(rootNode)) {
      return WindowRootNode.buildTree(rootNode);
    }

    const root = new BasicRootNode(rootNode);
    const childConstructor = (node) => BasicNode.create(node, root);

    BasicRootNode.findAndSetChildren(root, childConstructor);
    return root;
  }

  /**
   * Helper function to connect tree elements, given the root node and a
   * constructor for the child type.
   * @param {!BasicRootNode} root
   * @param {function(!AutomationNode): !SAChildNode} childConstructor
   *     Constructs a child node from an automation node.
   */
  static findAndSetChildren(root, childConstructor) {
    const interestingChildren = BasicRootNode.getInterestingChildren(root);
    const children = interestingChildren.map(childConstructor)
                         .filter((child) => child.isValidAndVisible());

    if (children.length < 1) {
      throw SwitchAccess.error(
          SAConstants.ErrorType.NO_CHILDREN,
          'Root node must have at least 1 interesting child.',
          true /* shouldRecover */);
    }
    children.push(new BackButtonNode(root));
    root.children = children;
  }

  /**
   * @param {!BasicRootNode|!AutomationNode} root
   * @return {!Array<!AutomationNode>}
   */
  static getInterestingChildren(root) {
    if (root instanceof BasicRootNode) {
      root = root.automationNode;
    }

    if (root.children.length === 0) {
      return [];
    }
    const interestingChildren = [];
    const treeWalker = new AutomationTreeWalker(
        root, constants.Dir.FORWARD, SwitchAccessPredicate.restrictions(root));
    let node = treeWalker.next().node;

    while (node) {
      interestingChildren.push(node);
      node = treeWalker.next().node;
    }

    return interestingChildren;
  }
}
