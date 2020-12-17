// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation intents for speech feedback.
 */

goog.provide('IntentHandler');

goog.require('editing.EditableLine');

goog.scope(function() {
const AutomationIntent = chrome.automation.AutomationIntent;
const IntentCommandType = chrome.automation.IntentCommandType;
const IntentTextBoundaryType = chrome.automation.IntentTextBoundaryType;

/**
 * A stateless class that turns intents into speech.
 */
IntentHandler = class {
  /**
   * Called when intents are received from an AutomationEvent.
   * @param {!Array<AutomationIntent>} intents
   * @param {!editing.EditableLine} cur The current line.
   * @param {editing.EditableLine} prev The previous line.
   * @return {boolean} Whether intents are handled.
   */
  static onIntents(intents, cur, prev) {
    if (intents.length === 0) {
      return false;
    }

    // Currently, discard all other intents once one is handled.
    for (let i = 0; i < intents.length; i++) {
      if (IntentHandler.onIntent(intents[i], cur, prev)) {
        return true;
      }
    }

    return false;
  }

  /**
   * Called when an intent is received.
   * @param {!AutomationIntent} intent
   * @param {!editing.EditableLine} cur The current line.
   * @param {editing.EditableLine} prev The previous line.
   * @return {boolean} Whether the intent was handled.
   */
  static onIntent(intent, cur, prev) {
    switch (intent.command) {
      case IntentCommandType.MOVE_SELECTION:
        return IntentHandler.onMoveSelection(intent, cur, prev);

        // TODO: implement support.
      case IntentCommandType.CLEAR_SELECTION:
      case IntentCommandType.DELETE:
      case IntentCommandType.DICTATE:
      case IntentCommandType.EXTEND_SELECTION:
      case IntentCommandType.FORMAT:
      case IntentCommandType.HISTORY:
      case IntentCommandType.INSERT:
      case IntentCommandType.MARKER:
      case IntentCommandType.SET_SELECTION:
        break;
    }

    return false;
  }

  /**
   * Called when the text selection moves.
   * @param {!AutomationIntent} intent A move selection
   *     intent.
   * @param {!editing.EditableLine} cur The current line.
   * @param {editing.EditableLine} prev The previous line.
   * @return {boolean} Whether the intent was handled.
   */
  static onMoveSelection(intent, cur, prev) {
    switch (intent.textBoundary) {
      case IntentTextBoundaryType.CHARACTER:
        // Read character to the right of the cursor. It is assumed to be a new
        // line if empty.
        // TODO: detect when this is the end of the document; read "end of text"
        // if so.
        ChromeVox.tts.speak(
            cur.text.substring(cur.startOffset, cur.startOffset + 1) || '\n',
            QueueMode.CATEGORY_FLUSH);
        return true;

      case IntentTextBoundaryType.LINE_END:
      case IntentTextBoundaryType.LINE_START:
      case IntentTextBoundaryType.LINE_START_OR_END:
        cur.speakLine(prev);
        return true;

        // TODO: implement support.
      case IntentTextBoundaryType.FORMAT:
      case IntentTextBoundaryType.OBJECT:
      case IntentTextBoundaryType.PAGE_END:
      case IntentTextBoundaryType.PAGE_START:
      case IntentTextBoundaryType.PAGE_START_OR_END:
      case IntentTextBoundaryType.PARAGRAPH_END:
      case IntentTextBoundaryType.PARAGRAPH_START:
      case IntentTextBoundaryType.PARAGRAPH_START_OR_END:
      case IntentTextBoundaryType.SENTENCE_END:
      case IntentTextBoundaryType.SENTENCE_START:
      case IntentTextBoundaryType.SENTENCE_START_OR_END:
      case IntentTextBoundaryType.WEB_PAGE:
      case IntentTextBoundaryType.WORD_END:
      case IntentTextBoundaryType.WORD_START:
      case IntentTextBoundaryType.WORD_START_OR_END:
        break;
    }

    return false;
  }
};
});  // goog.scope
