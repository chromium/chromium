// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../dictation_test_base.js']);

/** Dictation tests for speech parsing. */
DictationParseTest = class extends DictationE2ETestBase {};

// Tests that the InputTextStrategy always returns an InputTextViewMacro,
// regardless of the speech input.
SYNC_TEST_F('DictationParseTest', 'InputTextStrategy', async function() {
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
SYNC_TEST_F('DictationParseTest', 'SimpleParseStrategy', async function() {
  const strategy = this.getSimpleParseStrategy();
  assertNotNullNorUndefined(strategy);
  let macro = await strategy.parse('Hello world');
  assertEquals('INPUT_TEXT_VIEW', macro.getMacroNameString());
  macro = await strategy.parse('delete');
  assertEquals('DELETE_PREV_CHAR', macro.getMacroNameString());
  macro = await strategy.parse('move to the previous character');
  assertEquals('NAV_PREV_CHAR', macro.getMacroNameString());
  macro = await strategy.parse('move to the next character');
  assertEquals('NAV_NEXT_CHAR', macro.getMacroNameString());
  macro = await strategy.parse('move to the previous line');
  assertEquals('NAV_PREV_LINE', macro.getMacroNameString());
  macro = await strategy.parse('move to the next line');
  assertEquals('NAV_NEXT_LINE', macro.getMacroNameString());
  macro = await strategy.parse('copy');
  assertEquals('COPY_SELECTED_TEXT', macro.getMacroNameString());
  macro = await strategy.parse('paste');
  assertEquals('PASTE_TEXT', macro.getMacroNameString());
  macro = await strategy.parse('cut');
  assertEquals('CUT_SELECTED_TEXT', macro.getMacroNameString());
  macro = await strategy.parse('undo');
  assertEquals('UNDO_TEXT_EDIT', macro.getMacroNameString());
  macro = await strategy.parse('redo');
  assertEquals('REDO_ACTION', macro.getMacroNameString());
  macro = await strategy.parse('select all');
  assertEquals('SELECT_ALL_TEXT', macro.getMacroNameString());
  macro = await strategy.parse('unselect');
  assertEquals('UNSELECT_TEXT', macro.getMacroNameString());
  macro = await strategy.parse('help');
  assertEquals('LIST_COMMANDS', macro.getMacroNameString());
  macro = await strategy.parse('new line');
  assertEquals('NEW_LINE', macro.getMacroNameString());
});

// TODO(crbug.com/1264544): This test fails because of a memory issues
// when loading Pumpkin. The issue is not present in google3 test of Pumpkin
// WASM or when running Chrome with Dictation, so it is likely a limitation in
// the Chrome test framework. The test is only run when the default-false gn
// arg, enable_pumpkin_for_dictation, is set to true.
SYNC_TEST_F(
    'DictationParseTest', 'DISABLED_PumpkinDeleteCommand', async function() {
      const strategy = this.getPumpkinParseStrategy();
      if (!strategy) {
        return;
      }

      const macro = await strategy.parse('delete two characters');
      assertEquals('DELETE_PREV_CHAR', macro.getMacroNameString());
    });
