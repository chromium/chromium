// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox braille commands.
 */

goog.provide('BrailleCommandHandler');

goog.require('EventGenerator');
goog.require('EventSourceState');
goog.require('DesktopAutomationHandler');
goog.require('KeyCode');

goog.scope(function() {
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

/**
 * Global setting for the enabled state of this handler.
 * @param {boolean} state
 */
BrailleCommandHandler.setEnabled = function(state) {
  BrailleCommandHandler.enabled_ = state;
};

/**
 * Handles a braille command.
 * @param {!BrailleKeyEvent} evt
 * @param {!NavBraille} content
 * @return {boolean} True if evt was processed.
 */
BrailleCommandHandler.onBrailleKeyEvent = function(evt, content) {
  if (!BrailleCommandHandler.enabled_) {
    return true;
  }

  EventSourceState.set(EventSourceType.BRAILLE_KEYBOARD);

  // Note: panning within content occurs earlier in event dispatch.
  Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);
  switch (evt.command) {
    case BrailleKeyCommand.PAN_LEFT:
      CommandHandler.onCommand('previousObject');
      break;
    case BrailleKeyCommand.PAN_RIGHT:
      CommandHandler.onCommand('nextObject');
      break;
    case BrailleKeyCommand.LINE_UP:
      CommandHandler.onCommand('previousLine');
      break;
    case BrailleKeyCommand.LINE_DOWN:
      CommandHandler.onCommand('nextLine');
      break;
    case BrailleKeyCommand.TOP:
      CommandHandler.onCommand('jumpToTop');
      break;
    case BrailleKeyCommand.BOTTOM:
      CommandHandler.onCommand('jumpToBottom');
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
          CommandHandler.onCommand(command);
        }
      }
      break;
    default:
      return false;
  }
  return true;
};

/**
 * @param {!Spannable} text
 * @param {number} position
 * @private
 */
BrailleCommandHandler.onRoutingCommand_ = function(text, position) {
  let actionNodeSpan = null;
  let selectionSpan = null;
  const selSpans = text.getSpansInstanceOf(Output.SelectionSpan);
  const nodeSpans = text.getSpansInstanceOf(Output.NodeSpan);
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
    const start = interval ? interval.start : text.getSpanStart(selectionSpan);
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
};

/**
 * Customizes ChromeVox commands when issued from a braille display while within
 * editable text.
 * @param {string} command
 * @return {boolean} True if the command should propagate.
 * @private
 */
BrailleCommandHandler.onEditCommand_ = function(command) {
  const current = ChromeVoxState.instance.currentRange;
  if (ChromeVox.isStickyModeOn() || !current || !current.start ||
      !current.start.node || !current.start.node.state[StateType.EDITABLE]) {
    return true;
  }

  const textEditHandler = DesktopAutomationHandler.instance.textEditHandler;
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
};

/** @private {boolean} */
BrailleCommandHandler.enabled_ = true;
});  // goog.scope
