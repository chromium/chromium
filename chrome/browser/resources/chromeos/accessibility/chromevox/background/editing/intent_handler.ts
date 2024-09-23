// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation intents for speech feedback.
 * Braille is *not* handled in this module.
 */
import {AutomationPredicate} from '/common/automation_predicate.js';
import {AutomationUtil} from '/common/automation_util.js';
import {CursorRange} from '/common/cursors/range.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Output} from '../output/output.js';
import {OutputRoleInfo} from '../output/output_role_info.js';
import {OutputCustomEvent} from '../output/output_types.js';

import {EditableLine} from './editable_line.js';

type AutomationIntent = chrome.automation.AutomationIntent;
const IntentCommandType = chrome.automation.IntentCommandType;
const IntentTextBoundaryType = chrome.automation.IntentTextBoundaryType;
const RoleType = chrome.automation.RoleType;

/** A stateless class that turns intents into speech. */
export class IntentHandler {
  /**
   * Called when intents are received from an AutomationEvent.
   * @return Whether intents are handled.
   */
  static onIntents(
      intents: AutomationIntent[], cur: EditableLine, prev?: EditableLine)
      : boolean {
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
   * @return Whether the intent was handled.
   */
  static onIntent(
      intent: AutomationIntent, cur: EditableLine, prev?: EditableLine)
      : boolean {
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
   * @return Whether the intent was handled.
   */
  static onMoveSelection(
      intent: AutomationIntent, cur: EditableLine, prev?: EditableLine)
      : boolean {
    switch (intent.textBoundary) {
      case IntentTextBoundaryType.CHARACTER:
        return IntentHandler.onCharacterMoveSelection_(intent, cur, prev);
      case IntentTextBoundaryType.LINE_END:
      case IntentTextBoundaryType.LINE_START:
      case IntentTextBoundaryType.LINE_START_OR_END:
        // TODO(b/314203187): Not null asserted, check that this is correct.
        cur.speakLine(prev!);
        return true;
      case IntentTextBoundaryType.PARAGRAPH_START:
        return IntentHandler.onParagraphStartMoveSelection_(intent, cur, prev);
      case IntentTextBoundaryType.WORD_END:
      case IntentTextBoundaryType.WORD_START:
        return IntentHandler.onWordMoveSelection_(intent, cur, prev);

        // TODO: implement support.
      case IntentTextBoundaryType.FORMAT_END:
      case IntentTextBoundaryType.FORMAT_START:
      case IntentTextBoundaryType.FORMAT_START_OR_END:
      case IntentTextBoundaryType.OBJECT:
      case IntentTextBoundaryType.PAGE_END:
      case IntentTextBoundaryType.PAGE_START:
      case IntentTextBoundaryType.PAGE_START_OR_END:
      case IntentTextBoundaryType.PARAGRAPH_END:
      case IntentTextBoundaryType.PARAGRAPH_START_OR_END:
      case IntentTextBoundaryType.SENTENCE_END:
      case IntentTextBoundaryType.SENTENCE_START:
      case IntentTextBoundaryType.SENTENCE_START_OR_END:
      case IntentTextBoundaryType.WEB_PAGE:
      case IntentTextBoundaryType.WORD_START_OR_END:
        break;
    }

    return false;
  }

  /**
   * Called when the text selection moves with a boundary type of CHARACTER.
   * @return Whether the intent was handled.
   */
  private static onCharacterMoveSelection_(
      _intent: AutomationIntent, cur: EditableLine, prev?: EditableLine)
      : boolean {
    // Read character to the right of the cursor by building a character
    // range.
    let prevRange = undefined;
    if (prev) {
      prevRange = prev.createCharRange();
    }
    const newRange = cur.createCharRange();

    // Use the Output module for feedback so that we get contextual
    // information e.g. if we've entered a suggestion, insertion, or
    // deletion.
    const output = new Output();
    const text = cur.text;
    if (text.substring(cur.startOffset, cur.startOffset + 1).length === 0) {
      // There isn't any text to the right of the cursor.
      if (prev) {
        // Detect cases where |cur| is immediately before an abstractSpan.
        const enteredAncestors =
            AutomationUtil.getUniqueAncestors(prev.end.node, cur.end.node);
        const exitedAncestors =
            AutomationUtil.getUniqueAncestors(cur.end.node, prev.end.node);

        // Scan up only to a root or the editable root.
        let ancestor;
        const ancestors = enteredAncestors.concat(exitedAncestors);
        while ((ancestor = ancestors.pop()) &&
               !AutomationPredicate.rootOrEditableRoot(ancestor)) {
          // TODO(b/314203187): Not null asserted, check that this is correct.
          const roleInfo = OutputRoleInfo[ancestor.role!];
          if (roleInfo && roleInfo['inherits'] === 'abstractSpan') {
            // Let the caller handle this case.
            return false;
          }
        }
      }

      // This block special cases readout of the cursor when it reaches the
      // end of a line.
      if (text === '\u00a0') {
        output.withString('\u00a0');
      } else {
        // It is assumed to be a new line otherwise.
        output.withString('\n');
      }
    }

    output.withRichSpeech(newRange, prevRange, OutputCustomEvent.NAVIGATE).go();

    // Handled.
    return true;
  }

  /**
   * Called when the text selection moves with a boundary type of
   * PARAGRAPH_START.
   * @return Whether the intent was handled.
   */
  private static onParagraphStartMoveSelection_(
      _intent: AutomationIntent, cur: EditableLine, _prev?: EditableLine)
      : boolean {
    let node = cur.startContainer;

    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (node!.role === RoleType.LINE_BREAK) {
      return false;
    }

    while (node && AutomationPredicate.text(node)) {
      node = node.parent;
    }

    if (!node || node.role === RoleType.TEXT_FIELD) {
      return false;
    }

    new Output()
        .withRichSpeech(
            CursorRange.fromNode(node), undefined, OutputCustomEvent.NAVIGATE)
        .go();
    return true;
  }

  /**
   * Called when the text selection moves with a boundary type of WORD_START or
   * WORD_END.
   * @return Whether the intent was handled.
   */
  private static onWordMoveSelection_(
      intent: AutomationIntent, cur: EditableLine, prev?: EditableLine)
      : boolean {
    let prevRange = undefined;
    if (prev) {
      prevRange = prev.createWordRange(false);
    }

    const newRange = cur.createWordRange(
        intent.textBoundary === IntentTextBoundaryType.WORD_END);
    new Output()
        .withSpeech(newRange, prevRange, OutputCustomEvent.NAVIGATE)
        .go();
    return true;
  }
}

TestImportManager.exportForTesting(IntentHandler);
