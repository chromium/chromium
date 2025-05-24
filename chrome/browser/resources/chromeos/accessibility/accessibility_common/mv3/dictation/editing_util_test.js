// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['dictation_test_base.js']);

/** Test fixture for editing_util.js. */
DictationEditingUtilTest = class extends DictationE2ETestBase {};

AX_TEST_F('DictationEditingUtilTest', 'GetReplacePhraseData', function() {
  let value;
  let caretIndex;
  let deletePhrase;
  let result;
  const f = () =>
      EditingUtil.getReplacePhraseData(value, caretIndex, deletePhrase);

  // Simple delete.
  value = 'This is a difficult test';
  caretIndex = value.length;
  deletePhrase = 'difficult';
  result = f();
  assertEquals(9, result.newIndex);
  assertEquals(deletePhrase.length + 1, result.deleteLength);

  // Case-insensitive delete.
  value = 'This is a DIFFICULT test';
  caretIndex = value.length;
  deletePhrase = 'difficult';
  result = f();
  assertEquals(9, result.newIndex);
  assertEquals(deletePhrase.length + 1, result.deleteLength);

  // Delete when there are multiple instances of `deletePhrase`.
  value = 'The cow jumped over the moon';
  caretIndex = value.length;
  deletePhrase = 'the';
  result = f();
  assertEquals(19, result.newIndex);
  assertEquals(deletePhrase.length + 1, result.deleteLength);

  // Delete only content to the left of the caret.
  // "The cow| jumped over the moon"
  value = 'The cow jumped over the moon';
  caretIndex = 7;
  deletePhrase = 'the';
  result = f();
  assertEquals(0, result.newIndex);
  assertEquals(deletePhrase.length + 1, result.deleteLength);

  // Delete last word.
  value = 'The cow jumped over the moon.';
  caretIndex = value.length;
  deletePhrase = 'moon';
  result = f();
  assertEquals(23, result.newIndex);
  assertEquals(deletePhrase.length + 1, result.deleteLength);

  // Delete only at word boundaries.
  value = 'A square is also a rectangle';
  caretIndex = value.length;
  deletePhrase = 'a';
  result = f();
  assertEquals(16, result.newIndex);
  assertEquals(deletePhrase.length + 1, result.deleteLength);

  // Nothing is deleted if we can't find `deletePhrase`.
  value = 'This is a test';
  caretIndex = value.length;
  deletePhrase = 'coconut';
  result = f();
  assertEquals(null, result);

  // Nothing is deleted if the caret is at index 0.
  value = 'This is a test';
  caretIndex = 0;
  deletePhrase = 'test';
  result = f();
  assertEquals(null, result);

  // Nothing is deleted if the caret is in the middle of the matched phrase.
  // "A squ|are is also a rectangle".
  value = 'A square is also a rectangle';
  caretIndex = 5;
  deletePhrase = 'square';
  result = f();
  assertEquals(null, result);

  // Verify that we don't unexpectedly remove punctuation.
  value = 'Hello world.';
  caretIndex = value.length;
  deletePhrase = 'world';
  result = f();
  assertEquals(5, result.newIndex);
  assertEquals(deletePhrase.length + 1, result.deleteLength);

  // Nothing is deleted if `deletePhrase` includes punctuation.
  value = 'Hello world.';
  caretIndex = value.length;
  deletePhrase = 'world.';
  result = f();
  assertEquals(null, result);

  // Replace multiple words.
  value = 'The cow jumped over the moon';
  caretIndex = value.length;
  deletePhrase = 'jumped over the moon';
  result = f();
  assertEquals(7, result.newIndex);
  assertEquals(deletePhrase.length + 1, result.deleteLength);

  // Edge case: value is empty.
  value = '';
  caretIndex = 0;
  deletePhrase = 'coconut';
  result = f();
  assertEquals(null, result);

  // Edge case: caretIndex is negative.
  value = 'This is a test';
  caretIndex = -1;
  deletePhrase = 'test';
  result = f();
  assertEquals(null, result);

  // Edge case: caretIndex is larger than `value.length`. We treat this as
  // if the text caret is at the end of value.
  value = 'Hello';
  caretIndex = 5000;
  deletePhrase = 'Hello';
  result = f();
  assertEquals(0, result.newIndex);
  assertEquals(deletePhrase.length, result.deleteLength);
});

AX_TEST_F('DictationEditingUtilTest', 'GetInsertBeforeIndex', function() {
  let value;
  let caretIndex;
  let beforePhrase;
  let result;
  const f = () =>
      EditingUtil.getInsertBeforeIndex(value, caretIndex, beforePhrase);

  // Simple insert.
  value = 'This is a test.';
  caretIndex = value.length;
  beforePhrase = 'test';
  result = f();
  assertEquals(9, result);

  // Insert and match multiple words.
  value = 'This is a test';
  caretIndex = value.length;
  beforePhrase = 'This is a test';
  result = f();
  assertEquals(0, result);

  // Nothing is inserted if `beforePhrase` isn't present.
  value = 'This is a test';
  caretIndex = value.length;
  beforePhrase = 'coconut';
  result = f();
  assertEquals(-1, result);
});

AX_TEST_F('DictationEditingUtilTest', 'SelectBetween', function() {
  let value;
  let caretIndex;
  let startPhrase;
  let endPhrase;
  let selection;
  const f = () =>
      EditingUtil.selectBetween(value, caretIndex, startPhrase, endPhrase);

  // Select using words.
  value = 'This is a test.';
  caretIndex = value.length;
  startPhrase = 'is';
  endPhrase = 'test';
  selection = f();
  // "This |is a test|".
  assertEquals(5, selection.start);
  assertEquals(14, selection.end);

  // Select using phrases.
  value = 'The cow jumped over the moon';
  caretIndex = value.length;
  startPhrase = 'cow jumped';
  endPhrase = 'the moon';
  selection = f();
  // "The |cow jumped over the moon|".
  assertEquals(4, selection.start);
  assertEquals(value.length, selection.end);

  // Select the right-most occurrence of `startPhrase` and `endPhrase`.
  value = 'The cow jumped over the moon';
  caretIndex = value.length;
  startPhrase = 'the';
  endPhrase = 'moon';
  selection = f();
  // "The cow jumped over |the moon|".
  assertEquals(20, selection.start);
  assertEquals(value.length, selection.end);

  // `startPhrase` must be to the left of `endPhrase`.
  value = 'This is a test';
  caretIndex = value.length;
  startPhrase = 'test';
  endPhrase = 'is';
  selection = f();
  assertEquals(null, selection);

  // Both `startPhrase` and `endPhrase` must be to the left of the text caret.
  value = 'This is a test';
  caretIndex = '4';
  startPhrase = 'this';
  endPhrase = 'test';
  selection = f();
  assertEquals(null, selection);

  value = 'This is a test';
  caretIndex = '4';
  startPhrase = 'coconut';
  endPhrase = 'this';
  selection = f();
  assertEquals(null, selection);

  // `startPhrase` and `endPhrase` can be the same.
  value = 'This is a test';
  caretIndex = value.length;
  startPhrase = 'this';
  endPhrase = 'this';
  selection = f();
  assertEquals(0, selection.start);
  assertEquals(4, selection.end);

  // selectBetween works with punctuation.
  value = 'This?is.a,test#';
  caretIndex = value.length;
  startPhrase = 'is';
  endPhrase = 'a';
  selection = f();
  assertEquals(5, selection.start);
  assertEquals(9, selection.end);
});

AX_TEST_F('DictationEditingUtilTest', 'NavNextSent', function() {
  let value;
  let caretIndex;
  const f = () => EditingUtil.navNextSent(value, caretIndex);

  // Simple.
  value = 'Hello world. Goodnight world.';
  caretIndex = 0;
  assertEquals(12, f());

  // If the end of the sentence can't be found, then moving to the next sentence
  // should take us to the end of the value.
  value = 'Hello world goodnight world';
  caretIndex = 0;
  assertEquals(value.length, f());

  // Various punctuation.
  value = 'This?\nIs! A. Test;';
  caretIndex = 0;
  assertEquals(5, f());
  caretIndex = 5;
  assertEquals(9, f());
  caretIndex = 9;
  assertEquals(12, f());
  caretIndex = 12;
  assertEquals(value.length, f());


  // Edge case: empty value.
  value = '';
  caretIndex = 0;
  assertEquals(0, f());
});

AX_TEST_F('DictationEditingUtilTest', 'NavPrevSent', function() {
  let value;
  let caretIndex;
  const f = () => EditingUtil.navPrevSent(value, caretIndex);

  // Simple.
  value = 'Hello world. Goodnight world.';
  caretIndex = value.length;
  assertEquals(12, f());

  // If the end of the sentence can't be found, then moving to the previous
  // sentence should take us to the beginning of the value.
  value = 'Hello world goodnight world';
  caretIndex = value.length;
  assertEquals(0, f());

  // Various punctuation and whitespace.
  value = 'This?\nIs! A. Test;';
  caretIndex = value.length;
  assertEquals(12, f());
  caretIndex = 12;
  assertEquals(9, f());

  caretIndex = 9;
  assertEquals(5, f());
  caretIndex = 5;
  assertEquals(0, f());

  // Edge case: empty value.
  value = '';
  caretIndex = 0;
  assertEquals(0, f());
});

AX_TEST_F('DictationEditingUtilTest', 'SmartSpacing', function() {
  let value;
  let caretIndex;
  let commitText;
  const f = () => EditingUtil.smartSpacing(value, caretIndex, commitText);

  // Add an extra space.
  value = 'This is a test.';
  caretIndex = value.length;
  commitText = 'More text';
  assertEquals(' More text', f());

  value = 'This is a test';
  caretIndex = value.length;
  commitText = 'folks!';
  assertEquals(' folks!', f());

  // Don't add a space.
  value = 'This is a test. ';
  caretIndex = value.length;
  commitText = 'More text';
  assertEquals('More text', f());

  value = 'This is a test.';
  caretIndex = value.length;
  commitText = ' More text';
  assertEquals(' More text', f());

  value = 'This is a test';
  caretIndex = value.length;
  commitText = '!';
  assertEquals('!', f());

  // Test the behavior when inserting text in the middle of value.
  // A space should be prepended to `commitText`;
  // "This is a| test"
  value = 'This is a test';
  caretIndex = 9;
  commitText = 'simple';
  assertEquals(' simple', f());

  // A space should be appended to `commitText`;
  // "This is a |test"
  value = 'This is a test';
  caretIndex = 10;
  commitText = 'simple';
  assertEquals('simple ', f());

  // "This is|. a test"
  value = 'This is. a test';
  caretIndex = 7;
  commitText = 'simple';
  assertEquals(' simple ', f());

  // "hello|\nworld"; caret is right before the \n character.
  value = 'hello\nworld';
  caretIndex = 5;
  commitText = 'there';
  assertEquals(' there', f());

  // "hello\n|world"; caret is right after the \n character.
  value = 'hello\nworld';
  caretIndex = 6;
  commitText = 'there';
  assertEquals('there ', f());

  value = 'hello,';
  caretIndex = value.length;
  commitText = 'world';
  assertEquals(' world', f());

  // The below pattern is observed in an empty gmail compose box.
  value = '\n';
  caretIndex = value.length;
  commitText = 'Hello';
  assertEquals('Hello', f());
});

AX_TEST_F('DictationEditingUtilTest', 'SmartCapitalization', function() {
  let value;
  let caretIndex;
  let commitText;
  const f = () =>
      EditingUtil.smartCapitalization(value, caretIndex, commitText);

  value = 'Some text.';
  caretIndex = value.length;
  commitText = 'more text';
  assertEquals('More text', f());

  value = 'Some text';
  caretIndex = value.length;
  commitText = 'more text';
  assertEquals('more text', f());

  value = 'Some text   ';
  caretIndex = value.length;
  commitText = 'More text';
  assertEquals('more text', f());

  value = 'Some text.';
  caretIndex = value.length;
  commitText = 'More text';
  assertEquals('More text', f());

  value = 'Some text.\n';
  caretIndex = value.length;
  commitText = 'more text';
  assertEquals('More text', f());

  value = '';
  caretIndex = 0;
  commitText = 'more text';
  assertEquals('More text', f());

  value = 'This is a test';
  caretIndex = 9;
  commitText = 'biology';
  assertEquals('biology', f());

  value = 'hello,';
  caretIndex = value.length;
  commitText = 'world';
  assertEquals('world', f());

  // The below pattern is observed in an empty gmail compose box.
  value = '\n';
  caretIndex = value.length;
  commitText = 'hello';
  assertEquals('Hello', f());
});

AX_TEST_F('DictationEditingUtilTest', 'NavNextSentJa', function() {
  let value;
  let caretIndex;
  const f = () => EditingUtil.navNextSent(value, caretIndex);

  // Simple.
  value = '私はテニスが好きです。バスケットボールも好きです。';
  caretIndex = 0;
  assertEquals(11, f());

  // Middle of first sentence.
  value = '私はテニスが好きです。バスケットボールも好きです。';
  caretIndex = 4;
  assertEquals(11, f());

  // If the end of the sentence can't be found, then moving to the next sentence
  // should take us to the end of the value.
  value = '私はテニスが好きです。';
  caretIndex = 0;
  assertEquals(value.length, f());
});

AX_TEST_F('DictationEditingUtilTest', 'NavPrevSentJa', function() {
  let value;
  let caretIndex;
  const f = () => EditingUtil.navPrevSent(value, caretIndex);

  // Simple.
  value = '私はテニスが好きです。バスケットボールも好きです。';
  caretIndex = value.length;
  assertEquals(10, f());

  // Middle of second sentence.
  value = '私はテニスが好きです。バスケットボールも好きです。';
  caretIndex = 20;
  assertEquals(10, f());

  // If the end of the sentence can't be found, then moving to the previous
  // sentence should take us to the beginning of the value.
  value = '私はテニスが好きです。';
  caretIndex = value.length;
  assertEquals(0, f());
});
