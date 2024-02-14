// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LocalStorage} from '/common/local_storage.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Msgs} from '../../common/msgs.js';
import {Personality, QueueMode} from '../../common/tts_types.js';
import {ChromeVox} from '../chromevox.js';


/**
 * A list of typing echo options.
 * This defines the way typed characters get spoken.
 * CHARACTER: echoes typed characters.
 * WORD: echoes a word once a breaking character is typed (i.e. spacebar).
 * CHARACTER_AND_WORD: combines CHARACTER and WORD behavior.
 * NONE: speaks nothing when typing.
 * @enum
 */
export const TypingEchoState = {
  CHARACTER: 0,
  WORD: 1,
  CHARACTER_AND_WORD: 2,
  NONE: 3,
};
// STATE_COUNT is the number of possible echo levels.
const STATE_COUNT = Object.entries(TypingEchoState).length;

export class TypingEcho {
  static init() {
    if (TypingEcho.current !== undefined) {
      throw new Error('TypingEcho should only be initialized once.');
    }
    TypingEcho.current = /** @type {TypingEchoState} */ (
        LocalStorage.get('typingEcho', TypingEchoState.CHARACTER));
    LocalStorage.addListenerForKey(
        'typingEcho', newValue => TypingEcho.current = newValue);
  }
  /**
   * @param {number=} cur Current typing echo.
   * @return {number} Next typing echo.
   */
  static cycle(cur) {
    return ((cur ?? TypingEcho.current) + 1) % STATE_COUNT;
  }

  static cycleWithAnnouncement() {
    LocalStorage.set(
        'typingEcho', TypingEcho.cycle(LocalStorage.getNumber('typingEcho')));
    let announce = '';
    switch (LocalStorage.get('typingEcho')) {
      case TypingEchoState.CHARACTER:
        announce = Msgs.getMsg('character_echo');
        break;
      case TypingEchoState.WORD:
        announce = Msgs.getMsg('word_echo');
        break;
      case TypingEchoState.CHARACTER_AND_WORD:
        announce = Msgs.getMsg('character_and_word_echo');
        break;
      case TypingEchoState.NONE:
        announce = Msgs.getMsg('none_echo');
        break;
    }
    ChromeVox.tts.speak(announce, QueueMode.FLUSH, Personality.ANNOTATION);
  }

  /**
   * Return if characters should be spoken given the typing echo option.
   * @param {number} typingEcho Typing echo option.
   * @return {boolean} Whether the character should be spoken.
   */
  static shouldSpeakChar(typingEcho) {
    return typingEcho === TypingEchoState.CHARACTER_AND_WORD ||
        typingEcho === TypingEchoState.CHARACTER;
  }
}

/**
 * Stores the current choice of how ChromeVox should echo when entering text
 * into an editable text field.
 * @type {TypingEchoState}
 */
TypingEcho.current;

TestImportManager.exportForTesting(['TypingEchoState', TypingEchoState]);
