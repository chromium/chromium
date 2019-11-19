// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox braille command data.
 */

goog.provide('BrailleCommandData');

/**
 * Maps a dot pattern to a command.
 * @type {!Object<number, string>}
 */
BrailleCommandData.DOT_PATTERN_TO_COMMAND = {};

/**
 * Makes a dot pattern given a list of dots numbered from 1 to 8 arranged in a
 * braille cell (a 2 x 4 dot grid).
 * @param {Array<number>} dots The dots to be set in the returned pattern.
 * @return {number}
 */
BrailleCommandData.makeDotPattern = function(dots) {
  return dots.reduce(function(p, c) {
    return p | (1 << c - 1);
  }, 0);
};

/**
 * Gets a braille command based on a dot pattern from a chord.
 * @param {number} dots
 * @return {string?}
 */
BrailleCommandData.getCommand = function(dots) {
  var command = BrailleCommandData.DOT_PATTERN_TO_COMMAND[dots];
  return command;
};

/**
 * Gets a dot shortcut for a command.
 * @param {string} command
 * @param {boolean=} opt_chord True if the pattern comes from a chord.
 * @return {string} The shortcut.
 */
BrailleCommandData.getDotShortcut = function(command, opt_chord) {
  var commandDots = BrailleCommandData.getDots(command);
  return BrailleCommandData.makeShortcutText(commandDots, opt_chord);
};

/**
 * @param {number} pattern
 * @param {boolean=} opt_chord
 * @return {string}
 */
BrailleCommandData.makeShortcutText = function(pattern, opt_chord) {
  var dots = [];
  for (var shifter = 0; shifter <= 7; shifter++) {
    if ((1 << shifter) & pattern) {
      dots.push(shifter + 1);
    }
  }
  var msgid;
  if (dots.length > 1) {
    msgid = 'braille_dots';
  } else if (dots.length == 1) {
    msgid = 'braille_dot';
  }

  if (msgid) {
    var dotText = Msgs.getMsg(msgid, [dots.join('-')]);
    if (opt_chord) {
      dotText = Msgs.getMsg('braille_chord', [dotText]);
    }
    return dotText;
  }
  return '';
};

/**
 * @param {string} command
 * @return {number} The dot pattern for |command|.
 */
BrailleCommandData.getDots = function(command) {
  for (var key in BrailleCommandData.DOT_PATTERN_TO_COMMAND) {
    key = parseInt(key, 10);
    if (command == BrailleCommandData.DOT_PATTERN_TO_COMMAND[key]) {
      return key;
    }
  }
  return 0;
};

/**
 * @private
 */
BrailleCommandData.init_ = function() {
  var map = function(dots, command) {
    var pattern = BrailleCommandData.makeDotPattern(dots);
    var existingCommand = BrailleCommandData.DOT_PATTERN_TO_COMMAND[pattern];
    if (existingCommand) {
      throw 'Braille command pattern already exists: ' + dots + ' ' +
          existingCommand + '. Trying to map ' + command;
    }

    BrailleCommandData.DOT_PATTERN_TO_COMMAND[pattern] = command;
  };

  map([2, 3], 'previousGroup');
  map([5, 6], 'nextGroup');
  map([1], 'previousObject');
  map([4], 'nextObject');
  map([2], 'previousWord');
  map([5], 'nextWord');
  map([3], 'previousCharacter');
  map([6], 'nextCharacter');
  map([1, 2, 3], 'jumpToTop');
  map([4, 5, 6], 'jumpToBottom');

  map([1, 4], 'fullyDescribe');
  map([1, 3, 4], 'contextMenu');
  map([1, 2, 3, 5], 'readFromHere');
  map([2, 3, 4], 'toggleSelection');

  // Forward jump.
  map([1, 2], 'nextButton');
  map([1, 5], 'nextEditText');
  map([1, 2, 4], 'nextFormField');
  map([1, 2, 5], 'nextHeading');
  map([4, 5], 'nextLink');
  map([2, 3, 4, 5], 'nextTable');

  // Backward jump.
  map([1, 2, 7], 'previousButton');
  map([1, 5, 7], 'previousEditText');
  map([1, 2, 4, 7], 'previousFormField');
  map([1, 2, 5, 7], 'previousHeading');
  map([4, 5, 7], 'previousLink');
  map([2, 3, 4, 5, 7], 'previousTable');

  map([8], 'forceClickOnCurrentItem');
  map([3, 4], 'toggleSearchWidget');

  // Question.
  map([1, 4, 5, 6], 'toggleKeyboardHelp');

  // All cells (with 7 as mod).
  map([1, 2, 3, 4, 5, 6, 7], 'darkenScreen');
  map([1, 2, 3, 4, 5, 6], 'undarkenScreen');

  // s.
  map([1, 2, 3, 4, 5], 'toggleSpeechOnOrOff');

  // g.
  map([1, 2, 4, 5], 'toggleBrailleTable');
};

BrailleCommandData.init_();
