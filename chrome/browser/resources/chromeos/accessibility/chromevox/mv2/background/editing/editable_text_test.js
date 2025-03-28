// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

/**
 * A TTS class implementing speak and stop methods intended only for testing.
 * @constructor
 * @implements TtsInterface
 */
class TestTts {
  constructor() {
    /**
     * The strings that were spoken since the last call to get().
     * @private {Array<string>}
     */
    this.strings_ = [];

    /**
     * The last call of speak()'s speech property if any.
     * @private {TtsSpeechProperties|undefined}
     */
    this.lastProperties_;
  }

  /**
   * Returns the list of strings spoken since the last time this method was
   * called, and then clears the list.
   * @return {Array<string>} The list of strings.
   */
  getStrings() {
    var result = this.strings_;
    this.strings_ = [];
    return result;
  }

  /**
   * Returns the speech property from the last call of speak()'s if any.
   * @type {TtsSpeechProperties|undefined}
   */
  getLastProperties() {
    return this.lastProperties_;
  }

  /** @override */
  speak(text, queueMode, properties) {
    this.strings_.push(text);
    this.lastProperties_ = properties;
  }

  /** @override */
  isSpeaking() {
    return false;
  }

  /** @override */
  stop() {
    // Do nothing.
  }

  /** @override */
  increaseOrDecreaseProperty(propertyName, increase) {
    // Do nothing.
  }
}

/**
 * Stores the last braille content.
 * @constructor
 * @implements BrailleInterface
 */
class TestBraille {
  constructor() {
    this.content = null;
  }

  /** @override */
  write(params) {
    this.content = params;
  }
}

/**
 * Asserts the current braille content.
 *
 * @param {string} text Braille text.
 * @param {number=} opt_start Selection start.
 * @param {number=} opt_end Selection end.
 */
TestBraille.assertContent = function(text, opt_start, opt_end) {
  var c = ChromeVox.braille.content;
  assertTrue(c != null);
  opt_start = opt_start !== undefined ? opt_start : -1;
  opt_end = opt_end !== undefined ? opt_end : opt_start;
  assertEquals(text, c.text.toString());
  assertEquals(opt_start, c.startIndex);
  assertEquals(opt_end, c.endIndex);
};

/** Test fixture. */
ChromeVoxEditableTextUnitTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    LocalStorage.set('typingEcho', TypingEchoState.CHARACTER_AND_WORD);
    ChromeVoxEditableTextBase.eventTypingEcho = false;
    ChromeVoxEditableTextBase.shouldSpeakInsertions = true;
    ChromeVox.braille = new TestBraille();
  }
};

AX_TEST_F('ChromeVoxEditableTextUnitTest', 'CursorNavigation', function() {
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('Hello', 0, 0, false, tts);

  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello', 1, 1));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello', 2, 2));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello', 3, 3));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello', 4, 4));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello', 5, 5));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello', 4, 4));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello', 3, 3));
  assertEqualStringArrays(
      ['e', 'l', 'l', 'o', 'End of text', 'o', 'l'], tts.getStrings());
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello', 0, 0));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello', 5, 5));
  assertEqualStringArrays(['Hel', 'Hello'], tts.getStrings());
});

/** Test typing words. */
AX_TEST_F('ChromeVoxEditableTextUnitTest', 'TypingWords', function() {
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('', 0, 0, false, tts);
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('H', 1, 1));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('He', 2, 2));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hel', 3, 3));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hell', 4, 4));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello', 5, 5));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello,', 6, 6));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, ', 7, 7));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, W', 8, 8));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Wo', 9, 9));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Wor', 10, 10));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Worl', 11, 11));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, World', 12, 12));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, World.', 13, 13));
  assertEqualStringArrays(
      [
        'H',
        'e',
        'l',
        'l',
        'o',
        'Hello,',
        ' ',
        'W',
        'o',
        'r',
        'l',
        'd',
        'World.',
      ],
      tts.getStrings());

  // Backspace
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, World', 12, 12));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Worl', 11, 11));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Wor', 10, 10));
  assertEqualStringArrays(['.', 'd', 'l'], tts.getStrings());

  // Forward-delete
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Wor', 9, 9));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Wor', 8, 8));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Wor', 7, 7));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, or', 7, 7));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, r', 7, 7));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, ', 7, 7));
  assertEqualStringArrays(['r', 'o', 'W', 'o', 'r'], tts.getStrings());

  // Clear all
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('', 0, 0));
  assertEqualStringArrays(['Hello, , deleted'], tts.getStrings());

  // Paste / insert a whole word
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello', 5, 5));
  assertEqualStringArrays(['Hello'], tts.getStrings());
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, World', 12, 12));
  assertEqualStringArrays([', World'], tts.getStrings());
});

/** Test selection. */
AX_TEST_F('ChromeVoxEditableTextUnitTest', 'Selection', function() {
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('Hello, world.', 0, 0, false, tts);
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, world.', 0, 1));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, world.', 0, 2));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, world.', 0, 3));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, world.', 0, 4));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, world.', 0, 5));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, world.', 0, 6));
  assertEqualStringArrays(
      [
        'H',
        'selected',
        'e',
        'added to selection',
        'l',
        'added to selection',
        'l',
        'added to selection',
        'o',
        'added to selection',
        ',',
        'added to selection',
      ],
      tts.getStrings());
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, world.', 0, 12));
  assertEqualStringArrays([' world', 'added to selection'], tts.getStrings());
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, world.', 1, 12));
  assertEqualStringArrays(['H', 'removed from selection'], tts.getStrings());
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, world.', 2, 5));
  assertEqualStringArrays(['llo', 'selected'], tts.getStrings());
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, world.', 2, 2));
  assertEqualStringArrays(['llo', 'removed from selection'], tts.getStrings());
});


/**
 * Test autocomplete; suppose a user is typing "google.com/firefox" into an
 * address bar, and it's being autocompleted. Sometimes it's autocompleted
 * as they type, sometimes there's a short delay.
 */
AX_TEST_F('ChromeVoxEditableTextUnitTest', 'Autocomplete', function() {
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('', 0, 0, false, tts);

  // User types 'g'
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('g', 1, 1));
  assertEqualStringArrays(['g'], tts.getStrings());

  // The rest of 'google.com' is autocompleted and automatically selected.
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('google.com', 1, 10));
  assertEqualStringArrays(['oogle.com, oogle.com'], tts.getStrings());

  // The user doesn't realize it and types a few more characters of 'google.com'
  // and this changes the selection (unselecting) as the user types them.
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('google.com', 2, 10));
  assertEqualStringArrays(['o', 'ogle.com'], tts.getStrings());
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('google.com', 3, 10));
  assertEqualStringArrays(['o', 'gle.com'], tts.getStrings());
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('google.com', 4, 10));
  assertEqualStringArrays(['g', 'le.com'], tts.getStrings());

  // The user presses right-arrow, which fully unselects the remaining text.
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('google.com', 10, 10));
  assertEqualStringArrays(
      ['le.com', 'removed from selection'], tts.getStrings());

  // The user types '/'
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('google.com/', 11, 11));
  assertEqualStringArrays(['com/'], tts.getStrings());

  // The user types 'f', and 'finance' is autocompleted
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('google.com/finance', 12, 18));
  assertEqualStringArrays(['finance, inance'], tts.getStrings());

  // The user types 'i'
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('google.com/finance', 13, 18));
  assertEqualStringArrays(['i', 'nance'], tts.getStrings());

  // The user types 'r', now 'firefox' is autocompleted
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('google.com/firefox', 14, 18));
  assertEqualStringArrays(['refox, efox'], tts.getStrings());

  // The user presses right-arrow to accept the completion.
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('google.com/firefox', 18, 18));
  assertEqualStringArrays(['efox', 'removed from selection'], tts.getStrings());
});


/**
 * Test a few common scenarios where text is replaced.
 */
AX_TEST_F('ChromeVoxEditableTextUnitTest', 'ReplacingText', function() {
  // Initial value is Alabama.
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('Alabama', 0, 0, false, tts);

  // Entire text replaced with Alaska.
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Alaska', 0, 0));
  assertEqualStringArrays(['Alaska'], tts.getStrings());

  // Entire text selected.
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Alaska', 0, 6));
  assertEqualStringArrays(['Alaska', 'selected'], tts.getStrings());

  // Entire text replaced with Arizona.
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Arizona', 7, 7));
  assertEqualStringArrays(['Arizona'], tts.getStrings());

  // Entire text selected.
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Arizona', 0, 7));
  assertEqualStringArrays(['Arizona', 'selected'], tts.getStrings());

  // Click between 'r' and 'i'.
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Arizona', 2, 2));
  assertEqualStringArrays(
      ['Arizona', 'removed from selection'], tts.getStrings());

  // Next character removed from selection.
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Arizona', 2, 7));
  assertEqualStringArrays(['izona', 'selected'], tts.getStrings());

  // Selection replaced with "kansas" to make Arkansas.  This time it
  // says "kansas" because the deleted text was selected.
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Arkansas', 8, 8));
  assertEqualStringArrays(['kansas'], tts.getStrings());
});


/**
 * Test feedback when text changes in a long sentence.
 */
AX_TEST_F('ChromeVoxEditableTextUnitTest', 'ReplacingLongText', function() {
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase(
      'I love deadlines. I like the whooshing sound they make as they fly by.',
      0, 0, false, tts);

  // Change the whole sentence without moving the cursor. It should speak
  // only the part that changed, but it should speak whole words.
  AutomationEditableText.prototype.changed.call(
      obj,
      new TextChangeEvent(
          'I love deadlines. I love the whooshing sounds they make as they ' +
          'fly by.',
          0, 0));
  assertEqualStringArrays(['love the whooshing sounds'], tts.getStrings());
});

/** Tests character echo. */
AX_TEST_F('ChromeVoxEditableTextUnitTest', 'CharacterEcho', function() {
  LocalStorage.set('typingEcho', TypingEchoState.CHARACTER);
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('', 0, 0, false, tts);
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('H', 1, 1));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('He', 2, 2));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hel', 3, 3));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hell', 4, 4));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello', 5, 5));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello,', 6, 6));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, ', 7, 7));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, W', 8, 8));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Wo', 9, 9));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Wor', 10, 10));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Worl', 11, 11));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, World', 12, 12));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, World.', 13, 13));
  assertEqualStringArrays(
      ['H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '.'],
      tts.getStrings());
});


/** Tests character echo in auto complete text fields. */
AX_TEST_F(
    'ChromeVoxEditableTextUnitTest', 'CharEchoInAutoComplete', function() {
      var tts = new TestTts();
      var url = 'chromevox.com';
      var obj = new ChromeVoxEditableTextBase(url, 1, 13, false, tts);

      // This simulates a user typing into an auto complete text field one
      // character at a time. The selection is the completion and we toggle
      // between various typing echo options.
      LocalStorage.set('typingEcho', TypingEchoState.CHARACTER);
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent(url, 2, 13));
      LocalStorage.set('typingEcho', TypingEchoState.NONE);
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent(url, 3, 13));
      LocalStorage.set('typingEcho', TypingEchoState.CHARACTER_AND_WORD);
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent(url, 4, 13));
      LocalStorage.set('typingEcho', TypingEchoState.WORD);
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent(url, 5, 13));

      // The characters should only be read for the typing echo modes containing
      // a character. They are commented out below when unexpected to make the
      // test clearer to read.
      assertEqualStringArrays(
          [
            'h',
            url.slice(2),
            /* 'r', */ url.slice(3),
            'o',
            url.slice(4),
            /* 'm', */ url.slice(5),
          ],
          tts.getStrings());
    });


/** Tests word echo. */
AX_TEST_F('ChromeVoxEditableTextUnitTest', 'WordEcho', function() {
  LocalStorage.set('typingEcho', TypingEchoState.WORD);
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('', 0, 0, false, tts);
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('H', 1, 1));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('He', 2, 2));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hel', 3, 3));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hell', 4, 4));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello', 5, 5));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello,', 6, 6));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, ', 7, 7));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, W', 8, 8));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Wo', 9, 9));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Wor', 10, 10));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Worl', 11, 11));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, World', 12, 12));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, World.', 13, 13));
  assertEqualStringArrays(['Hello,', 'World.'], tts.getStrings());
});


/** Tests no echo. */
AX_TEST_F('ChromeVoxEditableTextUnitTest', 'NoEcho', function() {
  LocalStorage.set('typingEcho', TypingEchoState.NONE);
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('', 0, 0, false, tts);
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('H', 1, 1));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('He', 2, 2));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hel', 3, 3));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hell', 4, 4));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello', 5, 5));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello,', 6, 6));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, ', 7, 7));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, W', 8, 8));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Wo', 9, 9));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Wor', 10, 10));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, Worl', 11, 11));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, World', 12, 12));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('Hello, World.', 13, 13));
  assertEqualStringArrays([], tts.getStrings());
});

/** Tests normalization of TextChangeEvent's */
AX_TEST_F('ChromeVoxEditableTextUnitTest', 'TextChangeEvent', function() {
  var event1 = new TextChangeEvent('foo', 0, 1, true);
  var event2 = new TextChangeEvent('foo', 1, 0, true);
  var event3 = new TextChangeEvent('foo', 1, 1, true);

  assertEquals(0, event1.start);
  assertEquals(1, event1.end);

  assertEquals(0, event2.start);
  assertEquals(1, event2.end);

  assertEquals(1, event3.start);
  assertEquals(1, event3.end);
});

AX_TEST_F(
    'ChromeVoxEditableTextUnitTest', 'TypingNonBreakingSpaces', function() {
      var tts = new TestTts();
      var obj = new ChromeVoxEditableTextBase('Hello', 0, 0, false, tts);

      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('h', 1, 1));
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('hi', 2, 2));
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('hi\u00a0', 3, 3));
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('hi t', 4, 4));
      assertEqualStringArrays(['h', 'i', 'hi ', 't'], tts.getStrings());
    });

AX_TEST_F('ChromeVoxEditableTextUnitTest', 'DoesNotSpeakDeleted', function() {
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('Hello', 0, 0, false, tts);
  obj.multiline = true;

  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('wor', 0, 0));

  // This was once ['text_deleted'], but that is undesirable and mostly noise.
  assertEqualStringArrays([], tts.getStrings());
});

AX_TEST_F('ChromeVoxEditableTextUnitTest', 'IMETypingEcho', function() {
  LocalStorage.set('typingEcho', TypingEchoState.CHARACTER);
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('', 0, 0, false, tts);
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('ｋ', 1, 1));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('こ', 1, 1));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('こｎ', 2, 2));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('こん', 2, 2));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('こんｎ', 3, 3));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('こんに', 3, 3));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('こんにｃ', 4, 4));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('こんにｃｈ', 5, 5));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('こんにち', 4, 4));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('こんにちｈ', 5, 5));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('こんにちは', 5, 5));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('こんにちは！', 6, 6));
  assertEqualStringArrays(
      ['ｋ', 'こ', 'ｎ', 'ん', 'ｎ', 'に', 'ｃ', 'ｈ', 'ち', 'ｈ', 'は', '！'],
      tts.getStrings());
});

AX_TEST_F('ChromeVoxEditableTextUnitTest', 'IMETypingEchoLong', function() {
  LocalStorage.set('typingEcho', TypingEchoState.CHARACTER);
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('', 0, 0, false, tts);
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('ｘ', 1, 1));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('ｘｔ', 2, 2));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('ｘｔｓ', 3, 3));
  AutomationEditableText.prototype.changed.call(
      obj, new TextChangeEvent('っ', 1, 1));
  assertEqualStringArrays(['ｘ', 'ｔ', 'ｓ', 'っ'], tts.getStrings());
});

AX_TEST_F(
    'ChromeVoxEditableTextUnitTest', 'IMETypingEchoWithSuffix', function() {
      LocalStorage.set('typingEcho', TypingEchoState.CHARACTER);
      var tts = new TestTts();
      var obj = new ChromeVoxEditableTextBase('世界', 0, 0, false, tts);
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('ｋ世界', 1, 1));
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('こ世界', 1, 1));
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('こｎ世界', 2, 2));
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('こん世界', 2, 2));
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('こんｎ世界', 3, 3));
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('こんに世界', 3, 3));
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('こんにｃ世界', 4, 4));
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('こんにｃｈ世界', 5, 5));
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('こんにち世界', 4, 4));
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('こんにちｈ世界', 5, 5));
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('こんにちは世界', 5, 5));
      assertEqualStringArrays(
          ['ｋ', 'こ', 'ｎ', 'ん', 'ｎ', 'に', 'ｃ', 'ｈ', 'ち', 'ｈ', 'は'],
          tts.getStrings());
    });

AX_TEST_F(
    'ChromeVoxEditableTextUnitTest', 'StartCandidateSelection', function() {
      var tts = new TestTts();
      var obj =
          new ChromeVoxEditableTextBase('「こんにちは世界」', 6, 6, false, tts);

      // Assume that a user is composing "こんにちは".
      // When a user starts selection from IME conversion candidates, the cursor
      // position moves to the beginning of the composing region.
      AutomationEditableText.prototype.changed.call(
          obj, new TextChangeEvent('「今日は世界」', 1, 1));

      assertEqualStringArrays(['今日は'], tts.getStrings());
      assertNotNullNorUndefined(tts.getLastProperties());
      assertTrue(tts.getLastProperties()['phoneticCharacters']);
    });
