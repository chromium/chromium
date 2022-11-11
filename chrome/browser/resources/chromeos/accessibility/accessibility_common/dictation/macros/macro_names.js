// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Supported macros. Similar to UserIntent.MacroName in
 * google3/intelligence/dbw/proto/macros/user_intent.proto.
 * These should match semantic tags in Voice Access, see
 * voiceaccess_config.config and voiceaccess.patterns_template.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 * Ensure this enum stays in sync with the CrosDictationMacroName enum in
 * tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const MacroName = {
  UNSPECIFIED: 0,

  // Simply input text into a text field.
  INPUT_TEXT_VIEW: 1,

  // Delete one character.
  DELETE_PREV_CHAR: 2,

  // Move the cursor to the previous character.
  NAV_PREV_CHAR: 3,

  // Move the cursor to the next character.
  NAV_NEXT_CHAR: 4,

  // Move up to the previous line.
  NAV_PREV_LINE: 5,

  // Move down to the next line.
  NAV_NEXT_LINE: 6,

  // Copy any selected text, using clipboard copy.
  COPY_SELECTED_TEXT: 7,

  // Paste any clipboard text.
  PASTE_TEXT: 8,

  // Cut (copy and delete) any selected text.
  CUT_SELECTED_TEXT: 9,

  // Undo previous text-editing action. Does not undo
  // previous navigation or selection action, does not
  // clear clipboard.
  UNDO_TEXT_EDIT: 10,

  // Redo previous text-editing action. Does not redo
  // previous navigation or selection action, does not
  // clear clipboard.
  REDO_ACTION: 11,

  // Select all text in the text field.
  SELECT_ALL_TEXT: 12,

  // Clears the current selection, moving the cursor to
  // the end of the selection.
  UNSELECT_TEXT: 13,

  // Lists available Dictation commands by bringing up the Help page.
  LIST_COMMANDS: 14,

  // Insert a new line character.
  // Note: This doesn't correspond to a Voice Access action.
  NEW_LINE: 15,

  // Stops dictation.
  STOP_LISTENING: 16,

  // Delete one word.
  DELETE_PREV_WORD: 17,

  // Delete one sentence.
  DELETE_PREV_SENT: 18,

  // Move the cursor to the next word.
  NAV_NEXT_WORD: 19,

  // Move the cursor to the previous word.
  NAV_PREV_WORD: 20,

  // Deletes a provided word or phrase.
  SMART_DELETE_PHRASE: 21,

  // Replaces a provided word or phrase.
  SMART_REPLACE_PHRASE: 22,

  // Inserts a provided word or phrase.
  SMART_INSERT_BEFORE: 23,

  // Sets selection between two provided words or phrases.
  SMART_SELECT_BTWN_INCL: 24,

  // Move the cursor to the next sentence.
  NAV_NEXT_SENT: 25,

  // Move the cursor to the previous sentence.
  NAV_PREV_SENT: 26,

  // Deletes all text in the input field.
  DELETE_ALL_TEXT: 27,

  // Moves the cursor to the start of the input field.
  NAV_START_TEXT: 28,

  // Moves the cursor to the end of the input field.
  NAV_END_TEXT: 29,

  // Select the previous word in the input field.
  SELECT_PREV_WORD: 30,

  // Select the next word in the input field.
  SELECT_NEXT_WORD: 31,

  // Select the next character in the input field.
  SELECT_NEXT_CHAR: 32,

  // Select the previous character in the input field.
  SELECT_PREV_CHAR: 33,

  // Repeats the last executed macro.
  REPEAT: 34,

  // Any new actions should match with Voice Access's semantic tags.
};
