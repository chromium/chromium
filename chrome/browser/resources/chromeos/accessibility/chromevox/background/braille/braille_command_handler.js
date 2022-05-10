// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox braille commands.
 */
import {DesktopAutomationInterface} from '/chromevox/background/desktop_automation_interface.js';
import {BrailleCommandData} from '/chromevox/common/braille/braille_command_data.js';
import {EventGenerator} from '/common/event_generator.js';

const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

export class BrailleCommandHandler {
  /** @private */
  constructor() {
    /** @private {boolean} */
    this.enabled_ = true;
  }

  static get instance() {
    if (!BrailleCommandHandler.instance_) {
      BrailleCommandHandler.instance_ = new BrailleCommandHandler();
    }
    return BrailleCommandHandler.instance_;
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

    EventSourceState.set(EventSourceType.BRAILLE_KEYBOARD);

    // Try to restore to the last valid range.
    ChromeVoxState.instance.restoreLastValidRangeIfNeeded();

    // Note: panning within content occurs earlier in event dispatch.
    Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);
    switch (evt.command) {
      case BrailleKeyCommand.PAN_LEFT:
        CommandHandlerInterface.instance.onCommand('previousObject');
        break;
      case BrailleKeyCommand.PAN_RIGHT:
        CommandHandlerInterface.instance.onCommand('nextObject');
        break;
      case BrailleKeyCommand.LINE_UP:
        CommandHandlerInterface.instance.onCommand('previousLine');
        break;
      case BrailleKeyCommand.LINE_DOWN:
        CommandHandlerInterface.instance.onCommand('nextLine');
        break;
      case BrailleKeyCommand.TOP:
        CommandHandlerInterface.instance.onCommand('jumpToTop');
        break;
      case BrailleKeyCommand.BOTTOM:
        CommandHandlerInterface.instance.onCommand('jumpToBottom');
        break;
      case BrailleKeyCommand.ROUTING:
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
        focusOffset: targetPosition
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
    const current = ChromeVoxState.instance.currentRange;
    if (ChromeVox.isStickyModeOn() || !current || !current.start ||
        !current.start.node || !current.start.node.state[StateType.EDITABLE]) {
      return true;
    }

    const textEditHandler = DesktopAutomationInterface.instance.textEditHandler;
    if (!textEditHandler || current.start.node !== textEditHandler.node) {
      return true;
    }

    const isMultiline = AutomationPredicate.multiline(current.start.node);
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
BrailleCommandHandler.instance_;

BridgeHelper.registerHandler(
    BridgeTarget.BRAILLE_COMMAND_HANDLER, BridgeAction.SET_ENABLED,
    enabled => BrailleCommandHandler.setEnabled(enabled));
