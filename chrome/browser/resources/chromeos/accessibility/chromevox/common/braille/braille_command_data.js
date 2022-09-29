// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox braille command data.
 */

import {Command} from '../command_store.js';
import {Msgs} from '../msgs.js';

export const BrailleCommandData = {};

/**
 * Maps a dot pattern to a command.
 * @type {!Object<number, !Command>}
 */
BrailleCommandData.DOT_PATTERN_TO_COMMAND = {};

/**
 * Makes a dot pattern given a list of dots numbered from 1 to 8 arranged in a
 * braille cell (a 2 x 4 dot grid).
 * @param {Array<number>} dots The dots to be set in the returned pattern.
 * @return {number}
 */
BrailleCommandData.makeDotPattern = function(dots) {
  return dots.reduce((pattern, cell) => pattern | (1 << cell - 1), 0);
};

/**
 * Gets a braille command based on a dot pattern from a chord.
 * @param {number} dots
 * @return {?Command}
 */
BrailleCommandData.getCommand = function(dots) {
  return BrailleCommandData.DOT_PATTERN_TO_COMMAND[dots];
};

/**
 * Gets a dot shortcut for a command.
 * @param {!Command} command
 * @param {boolean=} opt_chord True if the pattern comes from a chord.
 * @return {string} The shortcut.
 */
BrailleCommandData.getDotShortcut = function(command, opt_chord) {
  const commandDots = BrailleCommandData.getDots(command);
  return BrailleCommandData.makeShortcutText(commandDots, opt_chord);
};

/**
 * @param {number} pattern
 * @param {boolean=} opt_chord
 * @return {string}
 */
BrailleCommandData.makeShortcutText = function(pattern, opt_chord) {
  const dots = [];
  for (let shifter = 0; shifter <= 7; shifter++) {
    if ((1 << shifter) & pattern) {
      dots.push(shifter + 1);
    }
  }
  let msgid;
  if (dots.length > 1) {
    msgid = 'braille_dots';
  } else if (dots.length === 1) {
    msgid = 'braille_dot';
  }

  if (msgid) {
    let dotText = Msgs.getMsg(msgid, [dots.join('-')]);
    if (opt_chord) {
      dotText = Msgs.getMsg('braille_chord', [dotText]);
    }
    return dotText;
  }
  return '';
};

/**
 * @param {!Command} command
 * @return {number} The dot pattern for |command|.
 */
BrailleCommandData.getDots = function(command) {
  for (let key in BrailleCommandData.DOT_PATTERN_TO_COMMAND) {
    key = parseInt(key, 10);
    if (command === BrailleCommandData.DOT_PATTERN_TO_COMMAND[key]) {
      return key;
    }
  }
  return 0;
};

/**
 * @private
 */
BrailleCommandData.init_ = function() {
  const map = function(dots, command) {
    const pattern = BrailleCommandData.makeDotPattern(dots);
    const existingCommand = BrailleCommandData.DOT_PATTERN_TO_COMMAND[pattern];
    if (existingCommand) {
      throw 'Braille command pattern already exists: ' + dots + ' ' +
          existingCommand + '. Trying to map ' + command;
    }

    BrailleCommandData.DOT_PATTERN_TO_COMMAND[pattern] = command;
  };

  map([2, 3], Command.PREVIOUS_GROUP);
  map([5, 6], Command.NEXT_GROUP);
  map([1], Command.PREVIOUS_OBJECT);
  map([4], Command.NEXT_OBJECT);
  map([2], Command.PREVIOUS_WORD);
  map([5], Command.NEXT_WORD);
  map([3], Command.PREVIOUS_CHARACTER);
  map([6], Command.NEXT_CHARACTER);
  map([1, 2, 3], Command.JUMP_TO_TOP);
  map([4, 5, 6], Command.JUMP_TO_BOTTOM);

  map([1, 4], Command.FULLY_DESCRIBE);
  map([1, 3, 4], Command.CONTEXT_MENU);
  map([1, 2, 3, 5], Command.READ_FROM_HERE);
  map([2, 3, 4], Command.TOGGLE_SELECTION);

  // Forward jump.
  map([1, 2], Command.NEXT_BUTTON);
  map([1, 5], Command.NEXT_EDIT_TEXT);
  map([1, 2, 4], Command.NEXT_FORM_FIELD);
  map([1, 2, 5], Command.NEXT_HEADING);
  map([4, 5], Command.NEXT_LINK);
  map([2, 3, 4, 5], Command.NEXT_TABLE);

  // Backward jump.
  map([1, 2, 7], Command.PREVIOUS_BUTTON);
  map([1, 5, 7], Command.PREVIOUS_EDIT_TEXT);
  map([1, 2, 4, 7], Command.PREVIOUS_FORM_FIELD);
  map([1, 2, 5, 7], Command.PREVIOUS_HEADING);
  map([4, 5, 7], Command.PREVIOUS_LINK);
  map([2, 3, 4, 5, 7], Command.PREVIOUS_TABLE);

  map([8], Command.FORCE_CLICK_ON_CURRENT_ITEM);
  map([3, 4], Command.TOGGLE_SEARCH_WIDGET);

  // Question.
  map([1, 4, 5, 6], Command.TOGGLE_KEYBOARD_HELP);

  // All cells.
  map([1, 2, 3, 4, 5, 6], Command.TOGGLE_SCREEN);

  // s.
  map([1, 2, 3, 4, 5], Command.TOGGLE_SPEECH_ON_OR_OFF);

  // g.
  map([1, 2, 4, 5], Command.TOGGLE_BRAILLE_TABLE);

  // Stop speech.
  map([5, 6, 7], Command.STOP_SPEECH);
};

BrailleCommandData.init_();
