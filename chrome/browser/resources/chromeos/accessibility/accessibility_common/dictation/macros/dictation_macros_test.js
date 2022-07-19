// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../dictation_test_base.js']);

/** Dictation tests for Macros. */
DictationMacrosTest = class extends DictationE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    await importModule(
        'MacroError', '/accessibility_common/dictation/macros/macro.js');
    await importModule(
        'StopListeningMacro',
        '/accessibility_common/dictation/macros/stop_listening_macro.js');
  }
};

AX_TEST_F('DictationMacrosTest', 'ValidInputTextViewMacro', async function() {
  // Toggle Dictation on so that the Macro will be runnable.
  this.toggleDictationOn();
  const macro = await this.getInputTextStrategy().parse('Hello world');
  assertEquals('INPUT_TEXT_VIEW', macro.getMacroNameString());
  const checkContextResult = macro.checkContext();
  assertTrue(checkContextResult.canTryAction);
  assertFalse(checkContextResult.willImmediatelyDisambiguate);
  assertEquals(undefined, checkContextResult.error);
  const runMacroResult = macro.runMacro();
  assertTrue(runMacroResult.isSuccess);
  assertEquals(undefined, runMacroResult.error);
  this.assertCommittedText('Hello world');
});

AX_TEST_F('DictationMacrosTest', 'InvalidInputTextViewMacro', async function() {
  // Do not toggle Dictation. The resulting macro will not be able to run.
  const macro = await this.getInputTextStrategy().parse('Hello world');
  assertEquals('INPUT_TEXT_VIEW', macro.getMacroNameString());
  const checkContextResult = macro.checkContext();
  assertFalse(checkContextResult.canTryAction);
  assertEquals(undefined, checkContextResult.willImmediatelyDisambiguate);
  assertEquals(MacroError.FAILED_ACTUATION, checkContextResult.error);
  const runMacroResult = macro.runMacro();
  assertFalse(runMacroResult.isSuccess);
  assertEquals(MacroError.FAILED_ACTUATION, runMacroResult.error);
});

AX_TEST_F('DictationMacrosTest', 'RepeatableKeyPressMacro', async function() {
  // DELETE_PREV_CHAR is one of many RepeatableKeyPressMacros.
  const macro = await this.getSimpleParseStrategy().parse('delete');
  assertEquals('DELETE_PREV_CHAR', macro.getMacroNameString());
  const checkContextResult = macro.checkContext();
  assertTrue(checkContextResult.canTryAction);
  assertFalse(checkContextResult.willImmediatelyDisambiguate);
  assertEquals(undefined, checkContextResult.error);
  const runMacroResult = macro.runMacro();
  assertTrue(runMacroResult.isSuccess);
  assertEquals(undefined, runMacroResult.error);
});

AX_TEST_F('DictationMacrosTest', 'ListCommandsMacro', async function() {
  this.toggleDictationOn();
  const macro = await this.getSimpleParseStrategy().parse('help');
  assertEquals('LIST_COMMANDS', macro.getMacroNameString());
  const checkContextResult = macro.checkContext();
  assertTrue(checkContextResult.canTryAction);
  assertFalse(checkContextResult.willImmediatelyDisambiguate);
  assertEquals(undefined, checkContextResult.error);
  const runMacroResult = macro.runMacro();
  assertTrue(runMacroResult.isSuccess);
  assertEquals(undefined, runMacroResult.error);
});

AX_TEST_F('DictationMacrosTest', 'StopListeningMacro', async function() {
  this.toggleDictationOn();
  assertTrue(this.getDictationActive());
  assertTrue(this.getSpeechRecognitionActive());
  const macro = new StopListeningMacro();
  assertEquals('STOP_LISTENING', macro.getMacroNameString());
  const checkContextResult = macro.checkContext();
  assertTrue(checkContextResult.canTryAction);
  assertFalse(checkContextResult.willImmediatelyDisambiguate);
  assertEquals(undefined, checkContextResult.error);
  const runMacroResult = macro.runMacro();
  assertTrue(runMacroResult.isSuccess);
  assertEquals(undefined, runMacroResult.error);
  assertFalse(this.getDictationActive());
  assertFalse(this.getSpeechRecognitionActive());
});

// Tests that smart macros can be parsed and constructed with the correct
// arguments.
SYNC_TEST_F('DictationMacrosTest', 'SmartMacros', async function() {
  const strategy = this.getSimpleParseStrategy();
  assertNotNullNorUndefined(strategy);
  let macro = await strategy.parse('delete hello world');
  assertEquals('SMART_DELETE_PHRASE', macro.getMacroNameString());
  assertEquals('hello world', macro.phrase_);
  macro = await strategy.parse('replace hello with goodbye');
  assertEquals('SMART_REPLACE_PHRASE', macro.getMacroNameString());
  assertEquals('hello', macro.deletePhrase_);
  assertEquals('goodbye', macro.insertPhrase_);
  macro = await strategy.parse('insert hello before goodbye');
  assertEquals('SMART_INSERT_BEFORE', macro.getMacroNameString());
  assertEquals('hello', macro.insertPhrase_);
  assertEquals('goodbye', macro.beforePhrase_);
  macro = await strategy.parse('select from hello to goodbye');
  assertEquals('SMART_SELECT_BTWN_INCL', macro.getMacroNameString());
  assertEquals('hello', macro.startPhrase_);
  assertEquals('goodbye', macro.endPhrase_);
});
