// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../dictation_test_base.js']);

/** Dictation tests for speech parsing. */
DictationParseTest = class extends DictationE2ETestBase {};

// Tests that the InputTextStrategy always returns an InputTextViewMacro,
// regardless of the speech input.
AX_TEST_F('DictationParseTest', 'InputTextStrategy', async function() {
  /** @type {!Array<!ParseTestCase>} */
  const testCases = [
    new ParseTestCase('Hello world', {name: 'INPUT_TEXT_VIEW'}),
    new ParseTestCase('delete two characters', {name: 'INPUT_TEXT_VIEW'}),
    new ParseTestCase('select all', {name: 'INPUT_TEXT_VIEW'}),
  ];

  for (const test of testCases) {
    await this.runInputTextParseTestCase(test);
  }
});

// Tests that the SimpleParseStrategy returns the correct type of Macro given
// speech input.
AX_TEST_F('DictationParseTest', 'SimpleParseStrategy', async function() {
  /** @type {!Array<!ParseTestCase>} */
  const testCases = [
    new ParseTestCase('Hello world', {name: 'INPUT_TEXT_VIEW'}),
    new ParseTestCase('type delete', {name: 'INPUT_TEXT_VIEW'}),
    new ParseTestCase('highlight the next word', {name: 'INPUT_TEXT_VIEW'}),
    new ParseTestCase('repeat', {name: 'INPUT_TEXT_VIEW'}),
    new ParseTestCase('delete', {name: 'DELETE_PREV_CHAR'}),
    new ParseTestCase(
        'move to the previous character', {name: 'NAV_PREV_CHAR'}),
    new ParseTestCase('move to the next character', {name: 'NAV_NEXT_CHAR'}),
    new ParseTestCase('move to the previous line', {name: 'NAV_PREV_LINE'}),
    new ParseTestCase('move to the next line', {name: 'NAV_NEXT_LINE'}),
    new ParseTestCase('copy', {name: 'COPY_SELECTED_TEXT'}),
    new ParseTestCase('paste', {name: 'PASTE_TEXT'}),
    new ParseTestCase('cut', {name: 'CUT_SELECTED_TEXT'}),
    new ParseTestCase('undo', {name: 'UNDO_TEXT_EDIT'}),
    new ParseTestCase('redo', {name: 'REDO_ACTION'}),
    new ParseTestCase('select all', {name: 'SELECT_ALL_TEXT'}),
    new ParseTestCase('unselect', {name: 'UNSELECT_TEXT'}),
    new ParseTestCase('help', {name: 'LIST_COMMANDS'}),
    new ParseTestCase('new line', {name: 'NEW_LINE'}),
    new ParseTestCase('cancel', {name: 'TOGGLE_DICTATION'}),
    new ParseTestCase('delete the previous word', {name: 'DELETE_PREV_WORD'}),
    new ParseTestCase('move to the next word', {name: 'NAV_NEXT_WORD'}),
    new ParseTestCase('move to the previous word', {name: 'NAV_PREV_WORD'}),
    new ParseTestCase(
        'delete the previous sentence',
        {name: 'DELETE_PREV_SENT', smart: true}),
    new ParseTestCase(
        'delete hello world', {name: 'SMART_DELETE_PHRASE', smart: true}),
    new ParseTestCase(
        'replace hello world with goodnight world',
        {name: 'SMART_REPLACE_PHRASE', smart: true}),
    new ParseTestCase(
        'insert hello world before goodnight world',
        {name: 'SMART_INSERT_BEFORE', smart: true}),
    new ParseTestCase(
        'select from hello world to goodnight world',
        {name: 'SMART_SELECT_BTWN_INCL', smart: true}),
    new ParseTestCase(
        'move to the next sentence', {name: 'NAV_NEXT_SENT', smart: true}),
    new ParseTestCase(
        'move to the previous sentence', {name: 'NAV_PREV_SENT', smart: true}),
  ];

  for (const test of testCases) {
    await this.runSimpleParseTestCase(test);
  }
});

AX_TEST_F('DictationParseTest', 'NoSmartMacrosForRTLLocales', async function() {
  await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'en-US');
  await this.getPref(Dictation.DICTATION_LOCALE_PREF);

  // Add is smart here and below.
  await this.runSimpleParseTestCase(new ParseTestCase(
      'insert hello world before goodnight world',
      {name: 'SMART_INSERT_BEFORE', smart: true}));

  // Change Dictation locale to a right-to-left locale.
  await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'ar-LB');
  await this.getPref(Dictation.DICTATION_LOCALE_PREF);

  // Smart macros are not supported in right-to-left locales. In these cases,
  // we fall back to INPUT_TEXT_VIEW macros.
  await this.runSimpleParseTestCase(new ParseTestCase(
      'insert hello world before goodnight world',
      {name: 'INPUT_TEXT_VIEW', smart: false}));
});
