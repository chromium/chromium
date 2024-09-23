// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventGenerator} from '/common/event_generator.js';
import {EventHandler} from '/common/event_handler.js';
import {RectUtil} from '/common/rect_util.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {AutoScanManager} from '../auto_scan_manager.js';
import {Navigator} from '../navigator.js';
import {SwitchAccess} from '../switch_access.js';
import {ActionResponse, ErrorType} from '../switch_access_constants.js';
import {SwitchAccessPredicate} from '../switch_access_predicate.js';

import {BackButtonNode} from './back_button_node.js';
import {BasicNode, BasicRootNode} from './basic_node.js';
import {GroupNode} from './group_node.js';
import {SAChildNode, SARootNode} from './switch_access_node.js';

type AutomationEvent = chrome.automation.AutomationEvent;
type AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
import MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;
const RoleType = chrome.automation.RoleType;

/**
 * This class handles the behavior of keyboard nodes directly associated with a
 * single AutomationNode.
 */
export class KeyboardNode extends BasicNode {
  static resetting = false;

  constructor(node: AutomationNode, parent: SARootNode) {
    super(node, parent);
  }

  // ================= Getters and setters =================

  override get actions(): MenuAction[] {
    return [MenuAction.SELECT];
  }

  // ================= General methods =================

  override asRootNode(): SARootNode | undefined {
    return undefined;
  }

  override isGroup(): boolean {
    return false;
  }

  override isValidAndVisible(): boolean {
    if (super.isValidAndVisible()) {
      return true;
    }
    if (!KeyboardNode.resetting &&
        Navigator.byItem.currentGroupHasChild(this)) {
      // TODO(crbug/1130773): move this code to another location, if possible
      KeyboardNode.resetting = true;
      KeyboardRootNode.ignoreNextExit = true;
      Navigator.byItem.exitKeyboard().then(
          () => Navigator.byItem.enterKeyboard());
    }

    return false;
  }

  override performAction(action: MenuAction): ActionResponse {
    if (action !== MenuAction.SELECT) {
      return ActionResponse.NO_ACTION_TAKEN;
    }

    const keyLocation = this.location;
    if (!keyLocation) {
      return ActionResponse.NO_ACTION_TAKEN;
    }

    // doDefault() does nothing on Virtual Keyboard buttons, so we must
    // simulate a mouse click.
    const center = RectUtil.center(keyLocation);
    EventGenerator.sendMouseClick(
        center.x, center.y, {delayMs: VK_KEY_PRESS_DURATION_MS});

    return ActionResponse.CLOSE_MENU;
  }
}

/**
 * This class handles the top-level Keyboard node, as well as the construction
 * of the Keyboard tree.
 */
export class KeyboardRootNode extends BasicRootNode {
  static ignoreNextExit = false;
  private static isVisible_ = false;
  private static explicitStateChange_ = false;
  private static object_?: AutomationNode;


  private constructor(groupNode: AutomationNode) {
    super(groupNode);
    KeyboardNode.resetting = false;
  }

  // ================= General methods =================

  override isValidGroup(): boolean {
    // To ensure we can find the keyboard root node to appropriately respond to
    // visibility changes, never mark it as invalid.
    return true;
  }

  override onExit(): void {
    if (KeyboardRootNode.ignoreNextExit) {
      KeyboardRootNode.ignoreNextExit = false;
      return;
    }

    // If the keyboard is currently visible, ignore the corresponding
    // state change.
    if (KeyboardRootNode.isVisible_) {
      KeyboardRootNode.explicitStateChange_ = true;
      chrome.accessibilityPrivate.setVirtualKeyboardVisible(false);
    }

    AutoScanManager.setInKeyboard(false);
  }

  override refreshChildren(): void {
    KeyboardRootNode.findAndSetChildren_(this);
  }

  // ================= Static methods =================

  /** Creates the tree structure for the keyboard. */
  static override buildTree(): KeyboardRootNode {
    KeyboardRootNode.loadKeyboard_();
    AutoScanManager.setInKeyboard(true);

    const keyboard = KeyboardRootNode.getKeyboardObject();
    if (!keyboard) {
      throw SwitchAccess.error(
          ErrorType.MISSING_KEYBOARD,
          'Could not find keyboard in the automation tree',
          true /* shouldRecover */);
    }
    const root = new KeyboardRootNode(keyboard);
    KeyboardRootNode.findAndSetChildren_(root);
    return root;
  }

  /** Start listening for keyboard open/closed. */
  static startWatchingVisibility(): void {
    const keyboardObject = KeyboardRootNode.getKeyboardObject();
    if (!keyboardObject) {
      SwitchAccess.findNodeMatching(
          {role: RoleType.KEYBOARD}, KeyboardRootNode.startWatchingVisibility);
      return;
    }

    KeyboardRootNode.isVisible_ = KeyboardRootNode.isKeyboardVisible_();

    new EventHandler(
        keyboardObject, EventType.LOAD_COMPLETE,
        KeyboardRootNode.checkVisibilityChanged_)
        .start();
    new EventHandler(
        keyboardObject, EventType.STATE_CHANGED,
        KeyboardRootNode.checkVisibilityChanged_, {exactMatch: true})
        .start();
  }

  // ================= Private static methods =================

  private static isKeyboardVisible_(): boolean {
    const keyboardObject = KeyboardRootNode.getKeyboardObject();
    return Boolean(
        keyboardObject && SwitchAccessPredicate.isVisible(keyboardObject) &&
        keyboardObject.find({role: RoleType.ROOT_WEB_AREA}));
  }

  private static checkVisibilityChanged_(_event: AutomationEvent): void {
    const currentlyVisible = KeyboardRootNode.isKeyboardVisible_();
    if (currentlyVisible === KeyboardRootNode.isVisible_) {
      return;
    }

    KeyboardRootNode.isVisible_ = currentlyVisible;

    if (KeyboardRootNode.explicitStateChange_) {
      // When the user has explicitly shown / hidden the keyboard, do not
      // enter / exit the keyboard again to avoid looping / double-calls.
      KeyboardRootNode.explicitStateChange_ = false;
      return;
    }

    if (KeyboardRootNode.isVisible_) {
      Navigator.byItem.enterKeyboard();
    } else {
      Navigator.byItem.exitKeyboard();
    }
  }

  /** Helper function to connect tree elements, given the root node. */
  private static findAndSetChildren_(root: KeyboardRootNode): void {
    const childConstructor =
        (node: AutomationNode): KeyboardNode => new KeyboardNode(node, root);
    const interestingChildren =
        root.automationNode.findAll({role: RoleType.BUTTON});
    const children: SAChildNode[] = GroupNode.separateByRow(
        interestingChildren.map(childConstructor), root.automationNode);

    children.push(new BackButtonNode(root));
    root.children = children;
  }

  private static getKeyboardObject(): AutomationNode {
    if (!this.object_ || !this.object_.role) {
      this.object_ = Navigator.byItem.desktopNode.find(
          {role: RoleType.KEYBOARD});
    }
    return this.object_;
  }

  /** Loads the keyboard. */
  private static loadKeyboard_(): void {
    if (KeyboardRootNode.isVisible_) {
      return;
    }

    chrome.accessibilityPrivate.setVirtualKeyboardVisible(true);
  }
}

BasicRootNode.builders.push({
  predicate: rootNode => rootNode.role === RoleType.KEYBOARD,
  builder: KeyboardRootNode.buildTree,
});

/**
 * The delay between keydown and keyup events on the virtual keyboard,
 * allowing the key press animation to display.
 */
const VK_KEY_PRESS_DURATION_MS = 100;

TestImportManager.exportForTesting(KeyboardNode, KeyboardRootNode);
