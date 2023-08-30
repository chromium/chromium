// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A list of typing echo options.
 * This defines the way typed characters get spoken.
 * CHARACTER: echoes typed characters.
 * WORD: echoes a word once a breaking character is typed (i.e. spacebar).
 * CHARACTER_AND_WORD: combines CHARACTER and WORD behavior.
 * NONE: speaks nothing when typing.
 * COUNT: The number of possible echo levels.
 * @enum
 */
export const TypingEcho = {
  CHARACTER: 0,
  WORD: 1,
  CHARACTER_AND_WORD: 2,
  NONE: 3,
  COUNT: 4,
};


/**
 * @param {number} cur Current typing echo.
 * @return {number} Next typing echo.
 */
TypingEcho.cycle = function(cur) {
  return (cur + 1) % TypingEcho.COUNT;
};


/**
 * Return if characters should be spoken given the typing echo option.
 * @param {number} typingEcho Typing echo option.
 * @return {boolean} Whether the character should be spoken.
 */
TypingEcho.shouldSpeakChar = function(typingEcho) {
  return typingEcho === TypingEcho.CHARACTER_AND_WORD ||
      typingEcho === TypingEcho.CHARACTER;
};
