// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../../common/testing/accessibility_test_base.js']);

/**
 * A TTS class implementing speak and stop methods intended only for testing.
 * @constructor
 * @implements TtsInterface
 */
class TestTts {
  constructor() {
    /**
     * The strings that were spoken since the last call to get().
     * @type {Array<string>}
     */
    this.strings = [];
  }

  /**
   * Returns the list of strings spoken since the last time this method was
   * called, and then clears the list.
   * @return {Array<string>} The list of strings.
   */
  get() {
    var result = this.strings;
    this.strings = [];
    return result;
  }

  /** @override */
  speak(text, queueMode, properties) {
    this.strings.push(text);
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
ChromeVoxEditableTextUnitTest = class extends AccessibilityTestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Alphabetical based on file path.
    await importModule(
        'ChromeVoxState', '/chromevox/background/chromevox_state.js');
    await importModule(
        ['ChromeVoxEditableTextBase', 'TextChangedEvent', 'TypingEcho'],
        '/chromevox/background/editing/editable_text_base.js');
    await importModule('LocalStorage', '/common/local_storage.js');

    LocalStorage.set('typingEcho', TypingEcho.CHARACTER_AND_WORD);
    ChromeVoxEditableTextBase.eventTypingEcho = false;
    ChromeVoxEditableTextBase.shouldSpeakInsertions = true;
    ChromeVox.braille = new TestBraille();

    /** Simple mock. */
    Msgs = {};

    /**
     * Simply return the message id.
     * @param {string} msg Message id.
     * @return {string} Message id.
     */
    Msgs.getMsg = function(msg) {
      return msg;
    };
  }
};

ChromeVoxEditableTextUnitTest.prototype.extraLibraries = [
  '../../../common/testing/assert_additions.js',
  '../../../common/closure_shim.js',
];


TEST_F('ChromeVoxEditableTextUnitTest', 'CursorNavigation', function() {
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('Hello', 0, 0, false, tts);

  obj.changed(new TextChangeEvent('Hello', 1, 1));
  obj.changed(new TextChangeEvent('Hello', 2, 2));
  obj.changed(new TextChangeEvent('Hello', 3, 3));
  obj.changed(new TextChangeEvent('Hello', 4, 4));
  obj.changed(new TextChangeEvent('Hello', 5, 5));
  obj.changed(new TextChangeEvent('Hello', 4, 4));
  obj.changed(new TextChangeEvent('Hello', 3, 3));
  assertEqualStringArrays(['H', 'e', 'l', 'l', 'o', 'o', 'l'], tts.get());
  obj.changed(new TextChangeEvent('Hello', 0, 0));
  obj.changed(new TextChangeEvent('Hello', 5, 5));
  assertEqualStringArrays(['Hel', 'Hello'], tts.get());
});

/** Test typing words. */
TEST_F('ChromeVoxEditableTextUnitTest', 'TypingWords', function() {
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('', 0, 0, false, tts);
  obj.changed(new TextChangeEvent('H', 1, 1));
  obj.changed(new TextChangeEvent('He', 2, 2));
  obj.changed(new TextChangeEvent('Hel', 3, 3));
  obj.changed(new TextChangeEvent('Hell', 4, 4));
  obj.changed(new TextChangeEvent('Hello', 5, 5));
  obj.changed(new TextChangeEvent('Hello,', 6, 6));
  obj.changed(new TextChangeEvent('Hello, ', 7, 7));
  obj.changed(new TextChangeEvent('Hello, W', 8, 8));
  obj.changed(new TextChangeEvent('Hello, Wo', 9, 9));
  obj.changed(new TextChangeEvent('Hello, Wor', 10, 10));
  obj.changed(new TextChangeEvent('Hello, Worl', 11, 11));
  obj.changed(new TextChangeEvent('Hello, World', 12, 12));
  obj.changed(new TextChangeEvent('Hello, World.', 13, 13));
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
      tts.get());

  // Backspace
  obj.changed(new TextChangeEvent('Hello, World', 12, 12));
  obj.changed(new TextChangeEvent('Hello, Worl', 11, 11));
  obj.changed(new TextChangeEvent('Hello, Wor', 10, 10));
  assertEqualStringArrays(['.', 'd', 'l'], tts.get());

  // Forward-delete
  obj.changed(new TextChangeEvent('Hello, Wor', 9, 9));
  obj.changed(new TextChangeEvent('Hello, Wor', 8, 8));
  obj.changed(new TextChangeEvent('Hello, Wor', 7, 7));
  obj.changed(new TextChangeEvent('Hello, or', 7, 7));
  obj.changed(new TextChangeEvent('Hello, r', 7, 7));
  obj.changed(new TextChangeEvent('Hello, ', 7, 7));
  assertEqualStringArrays(['r', 'o', 'W', 'W', 'o', 'r'], tts.get());

  obj.changed(new TextChangeEvent('Hello, Wor', 10, 10));
  obj.changed(new TextChangeEvent('Hello, Wor', 9, 9));
  obj.changed(new TextChangeEvent('Hello, Wor', 8, 8));
  obj.changed(new TextChangeEvent('Hello, Wor', 7, 7));
  obj.changed(new TextChangeEvent('Hello, or', 7, 7));
  obj.changed(new TextChangeEvent('Hello, r', 7, 7));
  obj.changed(new TextChangeEvent('Hello, ', 7, 7));
  assertEqualStringArrays(['Wor', 'r', 'o', 'W', 'o', 'r'], tts.get());

  // Clear all
  obj.changed(new TextChangeEvent('', 0, 0));
  assertEqualStringArrays(['Hello, , deleted'], tts.get());

  // Paste / insert a whole word
  obj.changed(new TextChangeEvent('Hello', 5, 5));
  assertEqualStringArrays(['Hello'], tts.get());
  obj.changed(new TextChangeEvent('Hello, World', 12, 12));
  assertEqualStringArrays([', World'], tts.get());
});

/** Test selection. */
TEST_F('ChromeVoxEditableTextUnitTest', 'Selection', function() {
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('Hello, world.', 0, 0, false, tts);
  obj.changed(new TextChangeEvent('Hello, world.', 0, 1));
  obj.changed(new TextChangeEvent('Hello, world.', 0, 2));
  obj.changed(new TextChangeEvent('Hello, world.', 0, 3));
  obj.changed(new TextChangeEvent('Hello, world.', 0, 4));
  obj.changed(new TextChangeEvent('Hello, world.', 0, 5));
  obj.changed(new TextChangeEvent('Hello, world.', 0, 6));
  assertEqualStringArrays(
      [
        'H',
        'selected',
        'e',
        'added_to_selection',
        'l',
        'added_to_selection',
        'l',
        'added_to_selection',
        'o',
        'added_to_selection',
        ',',
        'added_to_selection',
      ],
      tts.get());
  obj.changed(new TextChangeEvent('Hello, world.', 0, 12));
  assertEqualStringArrays([' world', 'added_to_selection'], tts.get());
  obj.changed(new TextChangeEvent('Hello, world.', 1, 12));
  assertEqualStringArrays(['H', 'removed_from_selection'], tts.get());
  obj.changed(new TextChangeEvent('Hello, world.', 2, 5));
  assertEqualStringArrays(['llo', 'selected'], tts.get());
  obj.changed(new TextChangeEvent('Hello, world.', 2, 2));
  assertEqualStringArrays(['llo', 'removed_from_selection'], tts.get());
});


/**
 * Test autocomplete; suppose a user is typing "google.com/firefox" into an
 * address bar, and it's being autocompleted. Sometimes it's autocompleted
 * as they type, sometimes there's a short delay.
 */
TEST_F('ChromeVoxEditableTextUnitTest', 'Autocomplete', function() {
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('', 0, 0, false, tts);

  // User types 'g'
  obj.changed(new TextChangeEvent('g', 1, 1));
  assertEqualStringArrays(['g'], tts.get());

  // The rest of 'google.com' is autocompleted and automatically selected.
  obj.changed(new TextChangeEvent('google.com', 1, 10));
  assertEqualStringArrays(['oogle.com, oogle.com'], tts.get());

  // The user doesn't realize it and types a few more characters of 'google.com'
  // and this changes the selection (unselecting) as the user types them.
  obj.changed(new TextChangeEvent('google.com', 2, 10));
  assertEqualStringArrays(['o', 'ogle.com'], tts.get());
  obj.changed(new TextChangeEvent('google.com', 3, 10));
  assertEqualStringArrays(['o', 'gle.com'], tts.get());
  obj.changed(new TextChangeEvent('google.com', 4, 10));
  assertEqualStringArrays(['g', 'le.com'], tts.get());

  // The user presses right-arrow, which fully unselects the remaining text.
  obj.changed(new TextChangeEvent('google.com', 10, 10));
  assertEqualStringArrays(['le.com', 'removed_from_selection'], tts.get());

  // The user types '/'
  obj.changed(new TextChangeEvent('google.com/', 11, 11));
  assertEqualStringArrays(['com/'], tts.get());

  // The user types 'f', and 'finance' is autocompleted
  obj.changed(new TextChangeEvent('google.com/finance', 12, 18));
  assertEqualStringArrays(['finance, inance'], tts.get());

  // The user types 'i'
  obj.changed(new TextChangeEvent('google.com/finance', 13, 18));
  assertEqualStringArrays(['i', 'nance'], tts.get());

  // The user types 'r', now 'firefox' is autocompleted
  obj.changed(new TextChangeEvent('google.com/firefox', 14, 18));
  assertEqualStringArrays(['refox, efox'], tts.get());

  // The user presses right-arrow to accept the completion.
  obj.changed(new TextChangeEvent('google.com/firefox', 18, 18));
  assertEqualStringArrays(['efox', 'removed_from_selection'], tts.get());
});


/**
 * Test a few common scenarios where text is replaced.
 */
TEST_F('ChromeVoxEditableTextUnitTest', 'ReplacingText', function() {
  // Initial value is Alabama.
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('Alabama', 0, 0, false, tts);

  // Entire text replaced with Alaska.
  obj.changed(new TextChangeEvent('Alaska', 0, 0));
  assertEqualStringArrays(['Alaska'], tts.get());

  // Entire text selected.
  obj.changed(new TextChangeEvent('Alaska', 0, 6));
  assertEqualStringArrays(['Alaska', 'selected'], tts.get());

  // Entire text replaced with Arizona.
  obj.changed(new TextChangeEvent('Arizona', 7, 7));
  assertEqualStringArrays(['Arizona'], tts.get());

  // Entire text selected.
  obj.changed(new TextChangeEvent('Arizona', 0, 7));
  assertEqualStringArrays(['Arizona', 'selected'], tts.get());

  // Click between 'r' and 'i'.
  obj.changed(new TextChangeEvent('Arizona', 2, 2));
  assertEqualStringArrays(['Arizona', 'removed_from_selection'], tts.get());

  // Next character removed from selection.
  obj.changed(new TextChangeEvent('Arizona', 2, 7));
  assertEqualStringArrays(['izona', 'selected'], tts.get());

  // Selection replaced with "kansas" to make Arkansas.  This time it
  // says "kansas" because the deleted text was selected.
  obj.changed(new TextChangeEvent('Arkansas', 8, 8));
  assertEqualStringArrays(['kansas'], tts.get());
});


/**
 * Test feedback when text changes in a long sentence.
 */
TEST_F('ChromeVoxEditableTextUnitTest', 'ReplacingLongText', function() {
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase(
      'I love deadlines. I like the whooshing sound they make as they fly by.',
      0, 0, false, tts);

  // Change the whole sentence without moving the cursor. It should speak
  // only the part that changed, but it should speak whole words.
  obj.changed(new TextChangeEvent(
      'I love deadlines. I love the whooshing sounds they make as they fly by.',
      0, 0));
  assertEqualStringArrays(['love the whooshing sounds'], tts.get());
});

/** Tests character echo. */
TEST_F('ChromeVoxEditableTextUnitTest', 'CharacterEcho', function() {
  LocalStorage.set('typingEcho', TypingEcho.CHARACTER);
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('', 0, 0, false, tts);
  obj.changed(new TextChangeEvent('H', 1, 1));
  obj.changed(new TextChangeEvent('He', 2, 2));
  obj.changed(new TextChangeEvent('Hel', 3, 3));
  obj.changed(new TextChangeEvent('Hell', 4, 4));
  obj.changed(new TextChangeEvent('Hello', 5, 5));
  obj.changed(new TextChangeEvent('Hello,', 6, 6));
  obj.changed(new TextChangeEvent('Hello, ', 7, 7));
  obj.changed(new TextChangeEvent('Hello, W', 8, 8));
  obj.changed(new TextChangeEvent('Hello, Wo', 9, 9));
  obj.changed(new TextChangeEvent('Hello, Wor', 10, 10));
  obj.changed(new TextChangeEvent('Hello, Worl', 11, 11));
  obj.changed(new TextChangeEvent('Hello, World', 12, 12));
  obj.changed(new TextChangeEvent('Hello, World.', 13, 13));
  assertEqualStringArrays(
      ['H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '.'],
      tts.get());
});


/** Tests character echo in auto complete text fields. */
TEST_F('ChromeVoxEditableTextUnitTest', 'CharEchoInAutoComplete', function() {
  var tts = new TestTts();
  var url = 'chromevox.com';
  var obj = new ChromeVoxEditableTextBase(url, 1, 13, false, tts);

  // This simulates a user typing into an auto complete text field one character
  // at a time. The selection is the completion and we toggle between various
  // typing echo options.
  LocalStorage.set('typingEcho', TypingEcho.CHARACTER);
  obj.changed(new TextChangeEvent(url, 2, 13));
  LocalStorage.set('typingEcho', TypingEcho.NONE);
  obj.changed(new TextChangeEvent(url, 3, 13));
  LocalStorage.set('typingEcho', TypingEcho.CHARACTER_AND_WORD);
  obj.changed(new TextChangeEvent(url, 4, 13));
  LocalStorage.set('typingEcho', TypingEcho.WORD);
  obj.changed(new TextChangeEvent(url, 5, 13));

  // The characters should only be read for the typing echo modes containing a
  // character. They are commented out below when unexpected to make the test
  // clearer to read.
  assertEqualStringArrays(
      [
        'h',
        url.slice(2),
        /* 'r', */ url.slice(3),
        'o',
        url.slice(4),
        /* 'm', */ url.slice(5),
      ],
      tts.get());
});


/** Tests word echo. */
TEST_F('ChromeVoxEditableTextUnitTest', 'WordEcho', function() {
  LocalStorage.set('typingEcho', TypingEcho.WORD);
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('', 0, 0, false, tts);
  obj.changed(new TextChangeEvent('H', 1, 1));
  obj.changed(new TextChangeEvent('He', 2, 2));
  obj.changed(new TextChangeEvent('Hel', 3, 3));
  obj.changed(new TextChangeEvent('Hell', 4, 4));
  obj.changed(new TextChangeEvent('Hello', 5, 5));
  obj.changed(new TextChangeEvent('Hello,', 6, 6));
  obj.changed(new TextChangeEvent('Hello, ', 7, 7));
  obj.changed(new TextChangeEvent('Hello, W', 8, 8));
  obj.changed(new TextChangeEvent('Hello, Wo', 9, 9));
  obj.changed(new TextChangeEvent('Hello, Wor', 10, 10));
  obj.changed(new TextChangeEvent('Hello, Worl', 11, 11));
  obj.changed(new TextChangeEvent('Hello, World', 12, 12));
  obj.changed(new TextChangeEvent('Hello, World.', 13, 13));
  assertEqualStringArrays(['Hello,', 'World.'], tts.get());
});


/** Tests no echo. */
TEST_F('ChromeVoxEditableTextUnitTest', 'NoEcho', function() {
  LocalStorage.set('typingEcho', TypingEcho.NONE);
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('', 0, 0, false, tts);
  obj.changed(new TextChangeEvent('H', 1, 1));
  obj.changed(new TextChangeEvent('He', 2, 2));
  obj.changed(new TextChangeEvent('Hel', 3, 3));
  obj.changed(new TextChangeEvent('Hell', 4, 4));
  obj.changed(new TextChangeEvent('Hello', 5, 5));
  obj.changed(new TextChangeEvent('Hello,', 6, 6));
  obj.changed(new TextChangeEvent('Hello, ', 7, 7));
  obj.changed(new TextChangeEvent('Hello, W', 8, 8));
  obj.changed(new TextChangeEvent('Hello, Wo', 9, 9));
  obj.changed(new TextChangeEvent('Hello, Wor', 10, 10));
  obj.changed(new TextChangeEvent('Hello, Worl', 11, 11));
  obj.changed(new TextChangeEvent('Hello, World', 12, 12));
  obj.changed(new TextChangeEvent('Hello, World.', 13, 13));
  assertEqualStringArrays([], tts.get());
});

/** Tests normalization of TextChangeEvent's */
TEST_F('ChromeVoxEditableTextUnitTest', 'TextChangeEvent', function() {
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

TEST_F('ChromeVoxEditableTextUnitTest', 'TypingNonBreakingSpaces', function() {
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('Hello', 0, 0, false, tts);

  obj.changed(new TextChangeEvent('h', 1, 1));
  obj.changed(new TextChangeEvent('hi', 2, 2));
  obj.changed(new TextChangeEvent('hi\u00a0', 3, 3));
  obj.changed(new TextChangeEvent('hi t', 4, 4));
  assertEqualStringArrays(['h', 'i', 'hi ', 't'], tts.get());
});
TEST_F('ChromeVoxEditableTextUnitTest', 'DoesNotSpeakDeleted', function() {
  var tts = new TestTts();
  var obj = new ChromeVoxEditableTextBase('Hello', 0, 0, false, tts);
  obj.multiline = true;

  obj.changed(new TextChangeEvent('wor', 0, 0));

  // This was once ['text_deleted'], but that is undesirable and mostly noise.
  assertEqualStringArrays([], tts.get());
});
