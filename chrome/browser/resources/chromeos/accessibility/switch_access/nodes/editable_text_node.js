// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventGenerator} from '../../common/event_generator.js';
import {EventHandler} from '../../common/event_handler.js';
import {KeyCode} from '../../common/key_code.js';
import {Navigator} from '../navigator.js';
import {SwitchAccess} from '../switch_access.js';
import {SAConstants, SwitchAccessMenuAction} from '../switch_access_constants.js';
import {SwitchAccessPredicate} from '../switch_access_predicate.js';
import {TextNavigationManager} from '../text_navigation_manager.js';

import {BasicNode} from './basic_node.js';
import {SAChildNode, SARootNode} from './switch_access_node.js';

const AutomationNode = chrome.automation.AutomationNode;

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
    const selectIndex = actions.indexOf(SwitchAccessMenuAction.SELECT);
    if (selectIndex >= 0) {
      actions.splice(selectIndex, 1);
    }

    actions.unshift(
        SwitchAccessMenuAction.KEYBOARD, SwitchAccessMenuAction.DICTATION);

    if (SwitchAccess.improvedTextInputEnabled()) {
      actions.push(
          SwitchAccessMenuAction.MOVE_CURSOR,
          SwitchAccessMenuAction.JUMP_TO_BEGINNING_OF_TEXT,
          SwitchAccessMenuAction.JUMP_TO_END_OF_TEXT,
          SwitchAccessMenuAction.MOVE_BACKWARD_ONE_CHAR_OF_TEXT,
          SwitchAccessMenuAction.MOVE_FORWARD_ONE_CHAR_OF_TEXT,
          SwitchAccessMenuAction.MOVE_BACKWARD_ONE_WORD_OF_TEXT,
          SwitchAccessMenuAction.MOVE_FORWARD_ONE_WORD_OF_TEXT,
          SwitchAccessMenuAction.MOVE_DOWN_ONE_LINE_OF_TEXT,
          SwitchAccessMenuAction.MOVE_UP_ONE_LINE_OF_TEXT);

      actions.push(SwitchAccessMenuAction.START_TEXT_SELECTION);
      if (TextNavigationManager.currentlySelecting()) {
        actions.push(SwitchAccessMenuAction.END_TEXT_SELECTION);
      }

      if (TextNavigationManager.selectionExists) {
        actions.push(SwitchAccessMenuAction.CUT, SwitchAccessMenuAction.COPY);
      }
      if (TextNavigationManager.clipboardHasData) {
        actions.push(SwitchAccessMenuAction.PASTE);
      }
    }
    return actions;
  }

  // ================= General methods =================

  /** @override */
  doDefaultAction() {
    this.performAction(SwitchAccessMenuAction.KEYBOARD);
  }

  /** @override */
  performAction(action) {
    switch (action) {
      case SwitchAccessMenuAction.KEYBOARD:
        Navigator.byItem.enterKeyboard();
        return SAConstants.ActionResponse.CLOSE_MENU;
      case SwitchAccessMenuAction.DICTATION:
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
        return SAConstants.ActionResponse.CLOSE_MENU;
      case SwitchAccessMenuAction.MOVE_CURSOR:
        return SAConstants.ActionResponse.OPEN_TEXT_NAVIGATION_MENU;

      case SwitchAccessMenuAction.CUT:
        EventGenerator.sendKeyPress(KeyCode.X, {ctrl: true});
        return SAConstants.ActionResponse.REMAIN_OPEN;
      case SwitchAccessMenuAction.COPY:
        EventGenerator.sendKeyPress(KeyCode.C, {ctrl: true});
        return SAConstants.ActionResponse.REMAIN_OPEN;
      case SwitchAccessMenuAction.PASTE:
        EventGenerator.sendKeyPress(KeyCode.V, {ctrl: true});
        return SAConstants.ActionResponse.REMAIN_OPEN;

      case SwitchAccessMenuAction.START_TEXT_SELECTION:
        TextNavigationManager.saveSelectStart();
        return SAConstants.ActionResponse.OPEN_TEXT_NAVIGATION_MENU;
      case SwitchAccessMenuAction.END_TEXT_SELECTION:
        TextNavigationManager.saveSelectEnd();
        return SAConstants.ActionResponse.EXIT_SUBMENU;

      case SwitchAccessMenuAction.JUMP_TO_BEGINNING_OF_TEXT:
        TextNavigationManager.jumpToBeginning();
        return SAConstants.ActionResponse.REMAIN_OPEN;
      case SwitchAccessMenuAction.JUMP_TO_END_OF_TEXT:
        TextNavigationManager.jumpToEnd();
        return SAConstants.ActionResponse.REMAIN_OPEN;
      case SwitchAccessMenuAction.MOVE_BACKWARD_ONE_CHAR_OF_TEXT:
        TextNavigationManager.moveBackwardOneChar();
        return SAConstants.ActionResponse.REMAIN_OPEN;
      case SwitchAccessMenuAction.MOVE_BACKWARD_ONE_WORD_OF_TEXT:
        TextNavigationManager.moveBackwardOneWord();
        return SAConstants.ActionResponse.REMAIN_OPEN;
      case SwitchAccessMenuAction.MOVE_DOWN_ONE_LINE_OF_TEXT:
        TextNavigationManager.moveDownOneLine();
        return SAConstants.ActionResponse.REMAIN_OPEN;
      case SwitchAccessMenuAction.MOVE_FORWARD_ONE_CHAR_OF_TEXT:
        TextNavigationManager.moveForwardOneChar();
        return SAConstants.ActionResponse.REMAIN_OPEN;
      case SwitchAccessMenuAction.MOVE_UP_ONE_LINE_OF_TEXT:
        TextNavigationManager.moveUpOneLine();
        return SAConstants.ActionResponse.REMAIN_OPEN;
      case SwitchAccessMenuAction.MOVE_FORWARD_ONE_WORD_OF_TEXT:
        TextNavigationManager.moveForwardOneWord();
        return SAConstants.ActionResponse.REMAIN_OPEN;
    }
    return super.performAction(action);
  }
}

BasicNode.creators.push({
  predicate: SwitchAccessPredicate.isTextInput,
  creator: (node, parentNode) => new EditableTextNode(node, parentNode),
});
