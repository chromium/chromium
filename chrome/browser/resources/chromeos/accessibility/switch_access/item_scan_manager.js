// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationUtil} from '../common/automation_util.js';
import {EventGenerator} from '../common/event_generator.js';
import {EventHandler} from '../common/event_handler.js';
import {RectUtil} from '../common/rect_util.js';
import {RepeatedEventHandler} from '../common/repeated_event_handler.js';
import {RepeatedTreeChangeHandler} from '../common/repeated_tree_change_handler.js';

import {ActionManager} from './action_manager.js';
import {AutoScanManager} from './auto_scan_manager.js';
import {FocusRingManager} from './focus_ring_manager.js';
import {FocusData, FocusHistory} from './history.js';
import {MenuManager} from './menu_manager.js';
import {Navigator} from './navigator.js';
import {ItemNavigatorInterface} from './navigator_interface.js';
import {BackButtonNode} from './nodes/back_button_node.js';
import {BasicNode, BasicRootNode} from './nodes/basic_node.js';
import {DesktopNode} from './nodes/desktop_node.js';
import {EditableTextNode} from './nodes/editable_text_node.js';
import {KeyboardRootNode} from './nodes/keyboard_node.js';
import {ModalDialogRootNode} from './nodes/modal_dialog_node.js';
import {SliderNode} from './nodes/slider_node.js';
import {SAChildNode, SARootNode} from './nodes/switch_access_node.js';
import {TabNode} from './nodes/tab_node.js';
import {SwitchAccess} from './switch_access.js';
import {SAConstants} from './switch_access_constants.js';
import {SwitchAccessPredicate} from './switch_access_predicate.js';

const AutomationNode = chrome.automation.AutomationNode;

/** This class handles navigation amongst the elements onscreen. */
export class ItemScanManager extends ItemNavigatorInterface {
  /**
   * @param {!AutomationNode} desktop
   */
  constructor(desktop) {
    super();

    /** @private {!AutomationNode} */
    this.desktop_ = desktop;

    /** @private {!SARootNode} */
    this.group_ = DesktopNode.build(this.desktop_);

    /** @private {!SAChildNode} */
    // TODO(crbug.com/1106080): It is possible for the firstChild to be a
    // window which is occluded, for example if Switch Access is turned on
    // when the user has several browser windows opened. We should either
    // dynamically pick this.node_'s initial value based on an occlusion check,
    // or ensure that we move away from occluded children as quickly as soon
    // as they are detected using an interval set in DesktopNode.
    this.node_ = this.group_.firstChild;

    /** @private {!FocusHistory} */
    this.history_ = new FocusHistory();

    /** @private {?FocusData} */
    this.suspendedGroup_ = null;

    /** @private {boolean} */
    this.ignoreFocusInKeyboard_ = false;

    this.init_();
  }

  // =============== ItemNavigatorInterface implementation ==============

  /** @override */
  currentGroupHasChild(node) {
    return this.group_.children.includes(node);
  }

  /** @override */
  enterGroup() {
    if (!this.node_.isGroup()) {
      return;
    }

    const newGroup = this.node_.asRootNode();
    if (newGroup) {
      this.history_.save(new FocusData(this.group_, this.node_));
      this.setGroup_(newGroup);
    }
  }

  /** @override */
  enterKeyboard() {
    this.ignoreFocusInKeyboard_ = true;
    this.node_.automationNode.focus();
    const keyboard = KeyboardRootNode.buildTree();
    this.jumpTo_(keyboard);
  }

  /** @override */
  exitGroupUnconditionally() {
    this.exitGroup_();
  }

  /** @override */
  exitIfInGroup(node) {
    if (this.group_.isEquivalentTo(node)) {
      this.exitGroup_();
    }
  }

  /** @override */
  exitKeyboard() {
    this.ignoreFocusInKeyboard_ = false;
    const isKeyboard = data => data.group instanceof KeyboardRootNode;
    // If we are not in the keyboard, do nothing.
    if (!(this.group_ instanceof KeyboardRootNode) &&
        !this.history_.containsDataMatchingPredicate(isKeyboard)) {
      return;
    }

    while (this.history_.peek() !== null) {
      if (this.group_ instanceof KeyboardRootNode) {
        this.exitGroup_();
        break;
      }
      this.exitGroup_();
    }

    chrome.automation.getFocus(focus => {
      // First, try to move back to the focused node.
      if (focus) {
        this.moveTo_(focus);
      } else {
        // Otherwise, move to anything that's valid based on the above history.
        this.moveToValidNode();
      }
    });
  }

  /** @override */
  forceFocusedNode(node) {
    // Check if they are exactly the same instance. Checking contents
    // equality is not sufficient in case the node has been repopulated
    // after a refresh.
    if (this.node_ !== node) {
      this.setNode_(node);
    }
  }

  /** @override */
  getTreeForDebugging(wholeTree = true) {
    if (!wholeTree) {
      console.log(this.group_.debugString(wholeTree));
      return this.group_;
    }

    const desktopRoot = DesktopNode.build(this.desktop_);
    console.log(desktopRoot.debugString(wholeTree, '', this.node_));
    return desktopRoot;
  }

  /** @override */
  jumpToSwitchAccessMenu() {
    const menuNode = MenuManager.menuAutomationNode;
    if (!menuNode) {
      return;
    }
    const menu = BasicRootNode.buildTree(menuNode);
    this.jumpTo_(menu, false /* shouldExitMenu */);
  }

  /** @override */
  moveBackward() {
    if (this.node_.isValidAndVisible()) {
      this.tryMoving(this.node_.previous, node => node.previous, this.node_);
    } else {
      this.moveToValidNode();
    }
  }

  /** @override */
  moveForward() {
    if (this.node_.isValidAndVisible()) {
      this.tryMoving(this.node_.next, node => node.next, this.node_);
    } else {
      this.moveToValidNode();
    }
  }

  /** @override */
  tryMoving(node, getNext, startingNode) {
    if (node === startingNode) {
      // This should only happen if the desktop contains exactly one interesting
      // child and all other children are windows which are occluded.
      // Unlikely to happen since we can always access the shelf.
      return;
    }

    if (!(node instanceof BasicNode)) {
      this.setNode_(node);
      return;
    }
    if (!SwitchAccessPredicate.isWindow(node.automationNode)) {
      this.setNode_(node);
      return;
    }
    const location = node.location;
    if (!location) {
      // Closure compiler doesn't realize we already checked isValidAndVisible
      // before calling tryMoving, so we need to explicitly check location here
      // so that RectUtil.center does not cause a closure error.
      this.moveToValidNode();
      return;
    }
    const center = RectUtil.center(location);
    // Check if the top center is visible as a proxy for occlusion. It's
    // possible that other parts of the window are occluded, but in Chrome we
    // can't drag windows off the top of the screen.
    this.desktop_.hitTestWithReply(center.x, location.top, hitNode => {
      if (AutomationUtil.isDescendantOf(hitNode, node.automationNode)) {
        this.setNode_(node);
      } else if (node.isValidAndVisible()) {
        this.tryMoving(getNext(node), getNext, startingNode);
      } else {
        this.moveToValidNode();
      }
    });
  }

  /** @override */
  moveToValidNode() {
    const nodeIsValid = this.node_.isValidAndVisible();
    const groupIsValid = this.group_.isValidGroup();

    if (nodeIsValid && groupIsValid) {
      return;
    }

    if (nodeIsValid && !(this.node_ instanceof BackButtonNode)) {
      // Our group has been invalidated. Move to this node to repair the
      // group stack.
      this.moveTo_(this.node_.automationNode);
      return;
    }

    const child = this.group_.firstValidChild();
    if (groupIsValid && child) {
      this.setNode_(child);
      return;
    }

    this.restoreFromHistory_();

    // Make sure the menu isn't open unless we're still in the menu.
    if (!this.group_.isEquivalentTo(MenuManager.menuAutomationNode)) {
      ActionManager.exitAllMenus();
    }
  }

  /** @override */
  restart() {
    const point = Navigator.byPoint.currentPoint;
    SwitchAccess.mode = SAConstants.Mode.ITEM_SCAN;
    this.desktop_.hitTestWithReply(point.x, point.y, node => {
      this.moveTo_(node);
    });
  }

  /** @override */
  restoreSuspendedGroup() {
    if (this.suspendedGroup_) {
      // Clearing the focus rings avoids having them re-animate to the same
      // position.
      FocusRingManager.clearAll();
      this.history_.save(new FocusData(this.group_, this.node_));
      this.loadFromData_(this.suspendedGroup_);
    }
  }

  /** @override */
  suspendCurrentGroup() {
    const data = new FocusData(this.group_, this.node_);
    this.exitGroup_();
    this.suspendedGroup_ = data;
  }

  /** @override */
  get currentNode() {
    this.moveToValidNode();
    return this.node_;
  }

  /** @override */
  get desktopNode() {
    return this.desktop_;
  }

  // =============== Event Handlers ==============

  /**
   * When focus shifts, move to the element. Find the closest interesting
   *     element to engage with.
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onFocusChange_(event) {
    if (SwitchAccess.mode === SAConstants.Mode.POINT_SCAN) {
      return;
    }

    // Ignore focus changes from our own actions.
    if (event.eventFrom === 'action') {
      return;
    }

    // To be safe, let's ignore focus when we're in the SA menu or over the
    // keyboard.
    if (this.ignoreFocusInKeyboard_ ||
        this.group_ instanceof KeyboardRootNode || MenuManager.isMenuOpen()) {
      return;
    }

    if (this.node_.isEquivalentTo(event.target)) {
      return;
    }
    this.moveTo_(event.target);
  }

  /**
   * When scroll position changes, ensure that the focus ring is in the
   * correct place and that the focused node / node group are valid.
   * @private
   */
  onScrollChange_() {
    if (SwitchAccess.mode === SAConstants.Mode.POINT_SCAN) {
      return;
    }

    if (this.node_.isValidAndVisible()) {
      // Update focus ring.
      FocusRingManager.setFocusedNode(this.node_);
    }
    this.group_.refresh();
    ActionManager.refreshMenuUnconditionally();
  }

  /**
   * When a menu is opened, jump focus to the menu.
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onModalDialog_(event) {
    if (SwitchAccess.mode === SAConstants.Mode.POINT_SCAN) {
      return;
    }

    const modalRoot = ModalDialogRootNode.buildTree(event.target);
    if (modalRoot.isValidGroup()) {
      this.jumpTo_(modalRoot);
    }
  }

  /**
   * When the automation tree changes, ensure the group and node we are
   * currently listening to are fresh. This is only called when the tree change
   * occurred on the node or group which are currently active.
   * @param {!chrome.automation.TreeChange} treeChange
   * @private
   */
  onTreeChange_(treeChange) {
    if (SwitchAccess.mode === SAConstants.Mode.POINT_SCAN) {
      return;
    }

    if (treeChange.type === chrome.automation.TreeChangeType.NODE_REMOVED) {
      this.group_.refresh();
      this.moveToValidNode();
    } else if (
        treeChange.type ===
        chrome.automation.TreeChangeType.SUBTREE_UPDATE_END) {
      this.group_.refresh();
    }
  }

  // =============== Private Methods ==============

  /** @private */
  exitGroup_() {
    this.group_.onExit();
    this.restoreFromHistory_();
  }

  /** @private */
  init_() {
    chrome.automation.getFocus(focus => {
      if (focus && this.history_.buildFromAutomationNode(focus)) {
        this.restoreFromHistory_();
      } else {
        this.group_.onFocus();
        this.node_.onFocus();
      }
    });

    new RepeatedEventHandler(
        this.desktop_, chrome.automation.EventType.FOCUS,
        event => this.onFocusChange_(event));

    // ARC++ fires SCROLL_POSITION_CHANGED.
    new RepeatedEventHandler(
        this.desktop_, chrome.automation.EventType.SCROLL_POSITION_CHANGED,
        () => this.onScrollChange_());

    // Web and Views use AXEventGenerator, which fires
    // separate horizontal and vertical events.
    new RepeatedEventHandler(
        this.desktop_,
        chrome.automation.EventType.SCROLL_HORIZONTAL_POSITION_CHANGED,
        () => this.onScrollChange_());
    new RepeatedEventHandler(
        this.desktop_,
        chrome.automation.EventType.SCROLL_VERTICAL_POSITION_CHANGED,
        () => this.onScrollChange_());

    new RepeatedTreeChangeHandler(
        chrome.automation.TreeChangeObserverFilter.ALL_TREE_CHANGES,
        treeChange => this.onTreeChange_(treeChange), {
          predicate: treeChange =>
              this.group_.findChild(treeChange.target) != null ||
              this.group_.isEquivalentTo(treeChange.target),
        });

    // The status tray fires a SHOW event when it opens.
    new EventHandler(
        this.desktop_,
        [
          chrome.automation.EventType.MENU_START,
          chrome.automation.EventType.SHOW,
        ],
        event => this.onModalDialog_(event))
        .start();
  }

  /**
   * Jumps Switch Access focus to a specified node, such as when opening a menu
   * or the keyboard. Does not modify the groups already in the group stack.
   * @param {!SARootNode} group
   * @param {boolean} shouldExitMenu
   * @private
   */
  jumpTo_(group, shouldExitMenu = true) {
    if (shouldExitMenu) {
      ActionManager.exitAllMenus();
    }

    this.history_.save(new FocusData(this.group_, this.node_));
    this.setGroup_(group);
  }

  /**
   * Moves Switch Access focus to a specified node, based on a focus shift or
   *     tree change event. Reconstructs the group stack to center on that node.
   *
   * This is a "permanent" move, while |jumpTo_| is a "temporary" move.
   *
   * @param {!AutomationNode} automationNode
   * @private
   */
  moveTo_(automationNode) {
    ActionManager.exitAllMenus();
    if (this.history_.buildFromAutomationNode(automationNode)) {
      this.restoreFromHistory_();
    }
  }

  /**
   * Restores the most proximal state from the history.
   * @private
   */
  restoreFromHistory_() {
    // retrieve() guarantees that the data's group is valid.
    this.loadFromData_(this.history_.retrieve());
  }

  /**
   * Extracts the focus and group from save data.
   * @param {!FocusData} data
   * @private
   */
  loadFromData_(data) {
    if (!data.group.isValidGroup()) {
      return;
    }

    // |data.focus| may not be a child of |data.group| anymore since
    // |data.group| updates when retrieving the history record. So |data.focus|
    // should not be used as the preferred focus node.
    const groupChildren = data.group.children;
    var focusTarget = null;
    for (var index = 0; index < groupChildren.length; ++index) {
      const child = groupChildren[index];
      if (child.isEquivalentTo(data.focus)) {
        focusTarget = child;
        break;
      }
    }

    if (focusTarget && focusTarget.isValidAndVisible()) {
      this.setGroup_(data.group, focusTarget);
    } else {
      this.setGroup_(data.group);
    }
  }

  /**
   * Set |this.group_| to |group|, and sets |this.node_| to either |opt_focus|
   * or |group.firstChild|.
   * @param {!SARootNode} group
   * @param {SAChildNode=} opt_focus
   * @private
   */
  setGroup_(group, opt_focus) {
    // Clear the suspended group, as it's only valid in its original context.
    this.suspendedGroup_ = null;

    this.group_.onUnfocus();
    this.group_ = group;
    this.group_.onFocus();

    const node = opt_focus || this.group_.firstValidChild();
    if (!node) {
      this.moveToValidNode();
      return;
    }

    // Check to see if the new node requires we try and focus a new window.
    chrome.automation.getFocus(currentAutomationFocus => {
      const newAutomationNode = node.automationNode;
      if (!newAutomationNode || !currentAutomationFocus) {
        return;
      }

      // First, if the current focus is a descendant of the new node or vice
      // versa, then we're done here.
      if (AutomationUtil.isDescendantOf(
              currentAutomationFocus, newAutomationNode) ||
          AutomationUtil.isDescendantOf(
              newAutomationNode, currentAutomationFocus)) {
        return;
      }

      // The current focus and new node do not have one another in their
      // ancestry; try to focus an ancestor window of the new node. In
      // particular, the parenting aura::Window of the views::Widget.
      let widget = newAutomationNode;
      while (widget &&
             (widget.role !== chrome.automation.RoleType.WINDOW ||
              widget.className !== 'Widget')) {
        widget = widget.parent;
      }

      if (widget && widget.parent) {
        widget.parent.focus();
      }
    });

    this.setNode_(node);
  }

  /**
   * Set |this.node_| to |node|, and update what is displayed onscreen.
   * @param {!SAChildNode} node
   * @private
   */
  setNode_(node) {
    if (!node.isValidAndVisible()) {
      this.moveToValidNode();
      return;
    }
    this.node_.onUnfocus();
    this.node_ = node;
    this.node_.onFocus();
    AutoScanManager.restartIfRunning();
  }
}
