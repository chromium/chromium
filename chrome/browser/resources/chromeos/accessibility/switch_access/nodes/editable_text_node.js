// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventGenerator} from '../../common/event_generator.js';
import {EventHandler} from '../../common/event_handler.js';
import {KeyCode} from '../../common/key_code.js';
import {Navigator} from '../navigator.js';
import {SwitchAccess} from '../switch_access.js';
import {ActionResponse} from '../switch_access_constants.js';
import {SwitchAccessPredicate} from '../switch_access_predicate.js';
import {TextNavigationManager} from '../text_navigation_manager.js';

import {BasicNode} from './basic_node.js';
import {SAChildNode, SARootNode} from './switch_access_node.js';

const AutomationNode = chrome.automation.AutomationNode;
const MenuAction = chrome.accessibilityPrivate.SwitchAccessMenuAction;

/**
 * This class handles interactions with editable text fields.
 */
export class EditableTextNode extends BasicNode {
  /**
   * @param {!AutomationNode} baseNode
   * @param {?SARootNode} parent
   */
  constructor(baseNode, parent) {
    super(baseNode, parent);
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    const actions = super.actions;
    // The SELECT action is used to press buttons, etc. For text inputs, the
    // equivalent action is KEYBOARD, which focuses the input and opens the
    // keyboard.
    const selectIndex = actions.indexOf(MenuAction.SELECT);
    if (selectIndex >= 0) {
      actions.splice(selectIndex, 1);
    }

    actions.unshift(MenuAction.KEYBOARD, MenuAction.DICTATION);

    if (SwitchAccess.improvedTextInputEnabled()) {
      actions.push(
          MenuAction.MOVE_CURSOR, MenuAction.JUMP_TO_BEGINNING_OF_TEXT,
          MenuAction.JUMP_TO_END_OF_TEXT,
          MenuAction.MOVE_BACKWARD_ONE_CHAR_OF_TEXT,
          MenuAction.MOVE_FORWARD_ONE_CHAR_OF_TEXT,
          MenuAction.MOVE_BACKWARD_ONE_WORD_OF_TEXT,
          MenuAction.MOVE_FORWARD_ONE_WORD_OF_TEXT,
          MenuAction.MOVE_DOWN_ONE_LINE_OF_TEXT,
          MenuAction.MOVE_UP_ONE_LINE_OF_TEXT);

      actions.push(MenuAction.START_TEXT_SELECTION);
      if (TextNavigationManager.currentlySelecting()) {
        actions.push(MenuAction.END_TEXT_SELECTION);
      }

      if (TextNavigationManager.selectionExists) {
        actions.push(MenuAction.CUT, MenuAction.COPY);
      }
      if (TextNavigationManager.clipboardHasData) {
        actions.push(MenuAction.PASTE);
      }
    }
    return actions;
  }

  // ================= General methods =================

  /** @override */
  doDefaultAction() {
    this.performAction(MenuAction.KEYBOARD);
  }

  /** @override */
  performAction(action) {
    switch (action) {
      case MenuAction.KEYBOARD:
        Navigator.byItem.enterKeyboard();
        return ActionResponse.CLOSE_MENU;
      case MenuAction.DICTATION:
        if (this.automationNode.state[chrome.automation.StateType.FOCUSED]) {
          chrome.accessibilityPrivate.toggleDictation();
        } else {
          new EventHandler(
              this.automationNode, chrome.automation.EventType.FOCUS,
              () => chrome.accessibilityPrivate.toggleDictation(),
              {exactMatch: true, listenOnce: true})
              .start();
          this.automationNode.focus();
        }
        return ActionResponse.CLOSE_MENU;
      case MenuAction.MOVE_CURSOR:
        return ActionResponse.OPEN_TEXT_NAVIGATION_MENU;

      case MenuAction.CUT:
        EventGenerator.sendKeyPress(KeyCode.X, {ctrl: true});
        return ActionResponse.REMAIN_OPEN;
      case MenuAction.COPY:
        EventGenerator.sendKeyPress(KeyCode.C, {ctrl: true});
        return ActionResponse.REMAIN_OPEN;
      case MenuAction.PASTE:
        EventGenerator.sendKeyPress(KeyCode.V, {ctrl: true});
        return ActionResponse.REMAIN_OPEN;

      case MenuAction.START_TEXT_SELECTION:
        TextNavigationManager.saveSelectStart();
        return ActionResponse.OPEN_TEXT_NAVIGATION_MENU;
      case MenuAction.END_TEXT_SELECTION:
        TextNavigationManager.saveSelectEnd();
        return ActionResponse.EXIT_SUBMENU;

      case MenuAction.JUMP_TO_BEGINNING_OF_TEXT:
        TextNavigationManager.jumpToBeginning();
        return ActionResponse.REMAIN_OPEN;
      case MenuAction.JUMP_TO_END_OF_TEXT:
        TextNavigationManager.jumpToEnd();
        return ActionResponse.REMAIN_OPEN;
      case MenuAction.MOVE_BACKWARD_ONE_CHAR_OF_TEXT:
        TextNavigationManager.moveBackwardOneChar();
        return ActionResponse.REMAIN_OPEN;
      case MenuAction.MOVE_BACKWARD_ONE_WORD_OF_TEXT:
        TextNavigationManager.moveBackwardOneWord();
        return ActionResponse.REMAIN_OPEN;
      case MenuAction.MOVE_DOWN_ONE_LINE_OF_TEXT:
        TextNavigationManager.moveDownOneLine();
        return ActionResponse.REMAIN_OPEN;
      case MenuAction.MOVE_FORWARD_ONE_CHAR_OF_TEXT:
        TextNavigationManager.moveForwardOneChar();
        return ActionResponse.REMAIN_OPEN;
      case MenuAction.MOVE_UP_ONE_LINE_OF_TEXT:
        TextNavigationManager.moveUpOneLine();
        return ActionResponse.REMAIN_OPEN;
      case MenuAction.MOVE_FORWARD_ONE_WORD_OF_TEXT:
        TextNavigationManager.moveForwardOneWord();
        return ActionResponse.REMAIN_OPEN;
    }
    return super.performAction(action);
  }
}

BasicNode.creators.push({
  predicate: SwitchAccessPredicate.isTextInput,
  creator: (node, parentNode) => new EditableTextNode(node, parentNode),
});
