// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['dictation_test_base.js']);

/** Test fixture for editing_util.js. */
DictationEditingUtilTest = class extends DictationE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule(
        'EditingUtil', '/accessibility_common/dictation/editing_util.js');
  }
};

AX_TEST_F('DictationEditingUtilTest', 'ReplacePhrase', function() {
  let value;
  let caretIndex;
  let deletePhrase;
  let insertPhrase;
  const f = () =>
      EditingUtil.replacePhrase(value, caretIndex, deletePhrase, insertPhrase);

  // Simple delete.
  value = 'This is a test';
  caretIndex = value.length;
  deletePhrase = 'difficult';
  insertPhrase = '';
  assertEquals('This is a test', f());

  // Case-insensitive delete.
  value = 'This is a DIFFICULT test';
  caretIndex = value.length;
  deletePhrase = 'difficult';
  insertPhrase = '';
  assertEquals('This is a test', f());

  // Delete when there are multiple instances of `deletePhrase`.
  value = 'The cow jumped over the moon';
  caretIndex = value.length;
  deletePhrase = 'the';
  insertPhrase = '';
  assertEquals('The cow jumped over moon', f());

  // Delete only content to the left of the caret.
  // "The cow| jumped over the moon"
  value = 'The cow jumped over the moon';
  caretIndex = 7;
  deletePhrase = 'the';
  insertPhrase = '';
  assertEquals('cow jumped over the moon', f());

  // Delete last word.
  value = 'The cow jumped over the moon.';
  caretIndex = value.length;
  deletePhrase = 'moon';
  insertPhrase = '';
  assertEquals('The cow jumped over the.', f());

  // Delete only at word boundaries.
  value = 'A square is also a rectangle';
  caretIndex = value.length;
  deletePhrase = 'a';
  insertPhrase = '';
  assertEquals('A square is also rectangle', f());

  // Nothing is deleted if we can't find `deletePhrase`.
  value = 'This is a test';
  caretIndex = value.length;
  deletePhrase = 'coconut';
  insertPhrase = '';
  assertEquals('This is a test', f());

  // Nothing is deleted if the caret is at index 0.
  value = 'This is a test';
  caretIndex = 0;
  deletePhrase = 'test';
  insertPhrase = '';
  assertEquals('This is a test', f());

  // Nothing is deleted if the caret is in the middle of the matched phrase.
  // "A squ|are is also a rectangle".
  value = 'A square is also a rectangle';
  caretIndex = 5;
  deletePhrase = 'square';
  insertPhrase = '';
  assertEquals('A square is also a rectangle', f());

  // Nothing is deleted if `deletePhrase` includes punctuation.
  value = 'Hello world.';
  caretIndex = value.length;
  deletePhrase = 'world.';
  insertPhrase = '';
  assertEquals('Hello world.', f());

  // Simple replacement.
  value = 'This is a difficult test';
  caretIndex = value.length;
  deletePhrase = 'difficult';
  insertPhrase = 'simple';
  assertEquals('This is a simple test', f());

  // Replace multiple words.
  value = 'The cow jumped over the moon';
  caretIndex = value.length;
  deletePhrase = 'jumped over the moon';
  insertPhrase = 'went to bed early';
  assertEquals('The cow went to bed early', f());

  // Edge case: value is empty.
  value = '';
  caretIndex = 0;
  deletePhrase = 'coconut';
  insertPhrase = '';
  assertEquals('', f());

  // Edge case: caretIndex is negative.
  value = 'This is a test';
  caretIndex = -1;
  deletePhrase = 'test';
  insertPhrase = '';
  assertEquals('This is a test', f());

  // Edge case: caretIndex is larger than `value.length`. We treat this as
  // if the text caret is at the end of value.
  value = 'Hello';
  caretIndex = 5000;
  deletePhrase = 'Hello';
  insertPhrase = '';
  assertEquals('', f());
});

AX_TEST_F('DictationEditingUtilTest', 'InsertBefore', function() {
  let value;
  let caretIndex;
  let insertPhrase;
  let beforePhrase;
  const f = () =>
      EditingUtil.insertBefore(value, caretIndex, insertPhrase, beforePhrase);

  // Simple insert.
  value = 'This is a test.';
  caretIndex = value.length;
  insertPhrase = 'simple';
  beforePhrase = 'test';
  assertEquals('This is a simple test.', f());

  // Insert and match multiple words.
  value = 'This is a test';
  caretIndex = value.length;
  insertPhrase = 'This is a drill';
  beforePhrase = 'This is a test';
  assertEquals('This is a drill This is a test', f());

  // Nothing is inserted if `beforePhrase` isn't present.
  value = 'This is a test';
  caretIndex = value.length;
  insertPhrase = 'pineapple';
  beforePhrase = 'coconut';
  assertEquals('This is a test', f());
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

AX_TEST_F('DictationEditingUtilTest', 'AdjustCommitText', function() {
  let value;
  let caretIndex;
  let commitText;
  const f = () => EditingUtil.adjustCommitText(value, caretIndex, commitText);

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
});
