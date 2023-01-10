// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox braille commands.
 */
import {AutomationPredicate} from '../../../common/automation_predicate.js';
import {AutomationUtil} from '../../../common/automation_util.js';
import {EventGenerator} from '../../../common/event_generator.js';
import {KeyCode} from '../../../common/key_code.js';
import {BrailleCommandData} from '../../common/braille/braille_command_data.js';
import {BrailleKeyCommand, BrailleKeyEvent} from '../../common/braille/braille_key_types.js';
import {NavBraille} from '../../common/braille/nav_braille.js';
import {BridgeConstants} from '../../common/bridge_constants.js';
import {BridgeHelper} from '../../common/bridge_helper.js';
import {Command} from '../../common/command_store.js';
import {EventSourceType} from '../../common/event_source_type.js';
import {Spannable} from '../../common/spannable.js';
import {QueueMode} from '../../common/tts_types.js';
import {ChromeVox} from '../chromevox.js';
import {ChromeVoxRange} from '../chromevox_range.js';
import {ChromeVoxState} from '../chromevox_state.js';
import {CommandHandlerInterface} from '../command_handler_interface.js';
import {DesktopAutomationInterface} from '../desktop_automation_interface.js';
import {EventSource} from '../event_source.js';
import {Output} from '../output/output.js';
import {OutputNodeSpan, OutputSelectionSpan} from '../output/output_types.js';
import {ChromeVoxPrefs} from '../prefs.js';

const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

export class BrailleCommandHandler {
  /** @private */
  constructor() {
    /** @private {boolean} */
    this.enabled_ = true;
  }

  static init() {
    BrailleCommandHandler.instance = new BrailleCommandHandler();

    BridgeHelper.registerHandler(
        BridgeConstants.BrailleCommandHandler.TARGET,
        BridgeConstants.BrailleCommandHandler.Action.SET_ENABLED,
        enabled => BrailleCommandHandler.setEnabled(enabled));
  }

  /**
   * Global setting for the enabled state of this handler.
   * @param {boolean} state
   */
  static setEnabled(state) {
    BrailleCommandHandler.instance.enabled_ = state;
  }

  /**
   * Handles a braille command.
   * @param {!BrailleKeyEvent} evt
   * @param {!NavBraille} content
   * @return {boolean} True if evt was processed.
   */
  static onBrailleKeyEvent(evt, content) {
    if (!BrailleCommandHandler.instance.enabled_) {
      return true;
    }

    EventSource.set(EventSourceType.BRAILLE_KEYBOARD);

    // Try to restore to the last valid range.
    ChromeVoxState.instance.restoreLastValidRangeIfNeeded();

    // Note: panning within content occurs earlier in event dispatch.
    Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);
    switch (evt.command) {
      case BrailleKeyCommand.PAN_LEFT:
        CommandHandlerInterface.instance.onCommand(Command.PREVIOUS_OBJECT);
        break;
      case BrailleKeyCommand.PAN_RIGHT:
        CommandHandlerInterface.instance.onCommand(Command.NEXT_OBJECT);
        break;
      case BrailleKeyCommand.LINE_UP:
        CommandHandlerInterface.instance.onCommand(Command.PREVIOUS_LINE);
        break;
      case BrailleKeyCommand.LINE_DOWN:
        CommandHandlerInterface.instance.onCommand(Command.NEXT_LINE);
        break;
      case BrailleKeyCommand.TOP:
        CommandHandlerInterface.instance.onCommand(Command.JUMP_TO_TOP);
        break;
      case BrailleKeyCommand.BOTTOM:
        CommandHandlerInterface.instance.onCommand(Command.JUMP_TO_BOTTOM);
        break;
      case BrailleKeyCommand.ROUTING:
        const textEditHandler =
            DesktopAutomationInterface.instance.textEditHandler;
        if (textEditHandler) {
          textEditHandler.injectInferredIntents([{
            command: chrome.automation.IntentCommandType.MOVE_SELECTION,
            textBoundary: chrome.automation.IntentTextBoundaryType.CHARACTER,
          }]);
        }
        BrailleCommandHandler.onRoutingCommand_(
            content.text,
            // Cast ok since displayPosition is always defined in this case.
            /** @type {number} */ (evt.displayPosition));
        break;
      case BrailleKeyCommand.CHORD:
        if (!evt.brailleDots) {
          return false;
        }

        const command = BrailleCommandData.getCommand(evt.brailleDots);
        if (command) {
          if (BrailleCommandHandler.onEditCommand_(command)) {
            CommandHandlerInterface.instance.onCommand(command);
          }
        }
        break;
      default:
        return false;
    }
    return true;
  }

  /**
   * @param {!Spannable} text
   * @param {number} position
   * @private
   */
  static onRoutingCommand_(text, position) {
    let actionNodeSpan = null;
    let selectionSpan = null;
    const selSpans = text.getSpansInstanceOf(OutputSelectionSpan);
    const nodeSpans = text.getSpansInstanceOf(OutputNodeSpan);
    for (let i = 0, selSpan; selSpan = selSpans[i]; i++) {
      if (text.getSpanStart(selSpan) <= position &&
          position < text.getSpanEnd(selSpan)) {
        selectionSpan = selSpan;
        break;
      }
    }

    let interval;
    for (let j = 0, nodeSpan; nodeSpan = nodeSpans[j]; j++) {
      const intervals = text.getSpanIntervals(nodeSpan);
      const tempInterval = intervals.find(function(innerInterval) {
        return innerInterval.start <= position && position <= innerInterval.end;
      });
      if (tempInterval) {
        actionNodeSpan = nodeSpan;
        interval = tempInterval;
      }
    }

    if (!actionNodeSpan) {
      return;
    }

    let actionNode = actionNodeSpan.node;
    const offset = actionNodeSpan.offset;
    if (actionNode.role === RoleType.INLINE_TEXT_BOX) {
      actionNode = actionNode.parent;
    }
    actionNode.doDefault();

    if (actionNode.role !== RoleType.STATIC_TEXT &&
        !actionNode.state[StateType.EDITABLE]) {
      return;
    }

    if (!selectionSpan) {
      selectionSpan = actionNodeSpan;
    }

    if (actionNode.state.richlyEditable) {
      const start =
          interval ? interval.start : text.getSpanStart(selectionSpan);
      const targetPosition = position - start + offset;
      chrome.automation.setDocumentSelection({
        anchorObject: actionNode,
        anchorOffset: targetPosition,
        focusObject: actionNode,
        focusOffset: targetPosition,
      });
    } else {
      const start = text.getSpanStart(selectionSpan);
      const targetPosition = position - start + offset;
      actionNode.setSelection(targetPosition, targetPosition);
    }
  }

  /**
   * Customizes ChromeVox commands when issued from a braille display while
   * within editable text.
   * @param {string} command
   * @return {boolean} True if the command should propagate.
   * @private
   */
  static onEditCommand_(command) {
    const current = ChromeVoxRange.current;
    if (ChromeVoxPrefs.isStickyModeOn() || !current || !current.start ||
        !current.start.node || !current.start.node.state[StateType.EDITABLE]) {
      return true;
    }

    const textEditHandler = DesktopAutomationInterface.instance.textEditHandler;
    const editable = AutomationUtil.getEditableRoot(current.start.node);
    if (!editable || !textEditHandler || editable !== textEditHandler.node) {
      return true;
    }

    const isMultiline = AutomationPredicate.multiline(editable);
    switch (command) {
      case 'forceClickOnCurrentItem':
        EventGenerator.sendKeyPress(KeyCode.RETURN);
        break;
      case 'previousCharacter':
        EventGenerator.sendKeyPress(KeyCode.LEFT);
        break;
      case 'nextCharacter':
        EventGenerator.sendKeyPress(KeyCode.RIGHT);
        break;
      case 'previousWord':
        EventGenerator.sendKeyPress(KeyCode.LEFT, {ctrl: true});
        break;
      case 'nextWord':
        EventGenerator.sendKeyPress(KeyCode.RIGHT, {ctrl: true});
        break;
      case 'previousObject':
      case 'previousLine':
        if (!isMultiline || textEditHandler.isSelectionOnFirstLine()) {
          return true;
        }
        EventGenerator.sendKeyPress(KeyCode.UP);
        break;
      case 'nextObject':
      case 'nextLine':
        if (!isMultiline) {
          return true;
        }

        if (textEditHandler.isSelectionOnLastLine()) {
          textEditHandler.moveToAfterEditText();
          return false;
        }

        EventGenerator.sendKeyPress(KeyCode.DOWN);
        break;
      case 'previousGroup':
        EventGenerator.sendKeyPress(KeyCode.UP, {ctrl: true});
        break;
      case 'nextGroup':
        EventGenerator.sendKeyPress(KeyCode.DOWN, {ctrl: true});
        break;
      default:
        return true;
    }
    return false;
  }
}

/** @type {BrailleCommandHandler} */
BrailleCommandHandler.instance;
