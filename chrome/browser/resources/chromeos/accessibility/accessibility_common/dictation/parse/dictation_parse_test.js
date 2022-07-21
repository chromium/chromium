// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../dictation_test_base.js']);

/** Dictation tests for speech parsing. */
DictationParseTest = class extends DictationE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule(
        'SpeechParser',
        '/accessibility_common/dictation/parse/speech_parser.js');
  }
};

// Tests that the InputTextStrategy always returns an InputTextViewMacro,
// regardless of the speech input.
AX_TEST_F('DictationParseTest', 'InputTextStrategy', async function() {
  const strategy = this.getInputTextStrategy();
  assertNotNullNorUndefined(strategy);
  let macro = await strategy.parse('Hello world');
  assertEquals('INPUT_TEXT_VIEW', macro.getMacroNameString());
  macro = await strategy.parse('delete two characters');
  assertEquals('INPUT_TEXT_VIEW', macro.getMacroNameString());
  macro = await strategy.parse('select all');
  assertEquals('INPUT_TEXT_VIEW', macro.getMacroNameString());
});

// Tests that the SimpleParseStrategy returns the correct type of Macro given
// speech input.
AX_TEST_F('DictationParseTest', 'SimpleParseStrategy', async function() {
  const strategy = this.getSimpleParseStrategy();
  assertNotNullNorUndefined(strategy);
  let macro = await strategy.parse('Hello world');
  assertEquals('INPUT_TEXT_VIEW', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('type delete');
  assertEquals('INPUT_TEXT_VIEW', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('delete');
  assertEquals('DELETE_PREV_CHAR', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('move to the previous character');
  assertEquals('NAV_PREV_CHAR', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('move to the next character');
  assertEquals('NAV_NEXT_CHAR', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('move to the previous line');
  assertEquals('NAV_PREV_LINE', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('move to the next line');
  assertEquals('NAV_NEXT_LINE', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('copy');
  assertEquals('COPY_SELECTED_TEXT', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('paste');
  assertEquals('PASTE_TEXT', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('cut');
  assertEquals('CUT_SELECTED_TEXT', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('undo');
  assertEquals('UNDO_TEXT_EDIT', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('redo');
  assertEquals('REDO_ACTION', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('select all');
  assertEquals('SELECT_ALL_TEXT', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('unselect');
  assertEquals('UNSELECT_TEXT', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('help');
  assertEquals('LIST_COMMANDS', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('new line');
  assertEquals('NEW_LINE', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('cancel');
  assertEquals('STOP_LISTENING', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('delete the previous word');
  assertEquals('DELETE_PREV_WORD', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('move to the next word');
  assertEquals('NAV_NEXT_WORD', macro.getMacroNameString());
  assertFalse(macro.isSmart());
  macro = await strategy.parse('move to the previous word');
  assertEquals('NAV_PREV_WORD', macro.getMacroNameString());
  assertFalse(macro.isSmart());

  // Smart macros.
  macro = await strategy.parse('delete the previous sentence');
  assertEquals('DELETE_PREV_SENT', macro.getMacroNameString());
  assertTrue(macro.isSmart());
  macro = await strategy.parse('delete hello world');
  assertEquals('SMART_DELETE_PHRASE', macro.getMacroNameString());
  assertTrue(macro.isSmart());
  macro = await strategy.parse('replace hello world with goodnight world');
  assertEquals('SMART_REPLACE_PHRASE', macro.getMacroNameString());
  assertTrue(macro.isSmart());
  macro = await strategy.parse('insert hello world before goodnight world');
  assertEquals('SMART_INSERT_BEFORE', macro.getMacroNameString());
  assertTrue(macro.isSmart());
  macro = await strategy.parse('select from hello world to goodnight world');
  assertEquals('SMART_SELECT_BTWN_INCL', macro.getMacroNameString());
  assertTrue(macro.isSmart());
  macro = await strategy.parse('move to the next sentence');
  assertEquals('NAV_NEXT_SENT', macro.getMacroNameString());
  assertTrue(macro.isSmart());
  macro = await strategy.parse('move to the previous sentence');
  assertEquals('NAV_PREV_SENT', macro.getMacroNameString());
  assertTrue(macro.isSmart());
});

// TODO(crbug.com/1264544): This test fails because of a memory issues
// when loading Pumpkin. The issue is not present in google3 test of Pumpkin
// WASM or when running Chrome with Dictation, so it is likely a limitation in
// the Chrome test framework. The test is only run when the default-false gn
// arg, enable_pumpkin_for_dictation, is set to true.
AX_TEST_F(
    'DictationParseTest', 'DISABLED_PumpkinDeleteCommand', async function() {
      const strategy = this.getPumpkinParseStrategy();
      if (!strategy) {
        return;
      }

      const macro = await strategy.parse('delete two characters');
      assertEquals('DELETE_PREV_CHAR', macro.getMacroNameString());
    });

AX_TEST_F('DictationParseTest', 'NoSmartMacrosForRTLLocales', async function() {
  const strategy = this.getSimpleParseStrategy();
  assertNotNullNorUndefined(strategy);
  await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'en-US');
  await this.getPref(Dictation.DICTATION_LOCALE_PREF);

  let macro = await strategy.parse('insert hello world before goodnight world');
  assertNotNullNorUndefined(macro);
  assertTrue(macro.isSmart());
  assertEquals('SMART_INSERT_BEFORE', macro.getMacroNameString());

  // Change Dictation locale to a right-to-left locale.
  await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'ar-LB');
  await this.getPref(Dictation.DICTATION_LOCALE_PREF);

  // Smart macros are not supported in right-to-left locales. In these cases,
  // we fall back to INPUT_TEXT_VIEW macros.
  macro = await strategy.parse('insert hello world before goodnight world');
  assertNotNullNorUndefined(macro);
  assertFalse(macro.isSmart());
  assertEquals('INPUT_TEXT_VIEW', macro.getMacroNameString());
});
