// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../dictation_test_base.js']);

/** Dictation tests for Macros. */
DictationMV2MacrosTest = class extends DictationE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    this.mockAccessibilityPrivate.enableFeatureForTest(
        'dictationContextChecking', true);
  }
};

AX_TEST_F(
    'DictationMV2MacrosTest', 'ValidInputTextViewMacro', async function() {
      // Toggle Dictation on so that the Macro will be runnable.
      this.toggleDictationOn();
      const macro = await this.getInputTextStrategy().parse('Hello world');
      assertEquals('INPUT_TEXT_VIEW', macro.getNameAsString());
      const checkContextResult = macro.checkContext();
      assertTrue(checkContextResult.canTryAction);
      assertEquals(undefined, checkContextResult.error);
      const runMacroResult = macro.run();
      assertTrue(runMacroResult.isSuccess);
      assertEquals(undefined, runMacroResult.error);
      this.assertCommittedText('Hello world');
    });

AX_TEST_F(
    'DictationMV2MacrosTest', 'InvalidInputTextViewMacro', async function() {
      // Do not toggle Dictation. The resulting macro will not be able to run.
      const macro = await this.getInputTextStrategy().parse('Hello world');
      assertEquals('INPUT_TEXT_VIEW', macro.getNameAsString());
      const checkContextResult = macro.checkContext();
      assertFalse(checkContextResult.canTryAction);
      assertEquals(MacroError.BAD_CONTEXT, checkContextResult.error);
      const runMacroResult = macro.run();
      assertFalse(runMacroResult.isSuccess);
      assertEquals(MacroError.FAILED_ACTUATION, runMacroResult.error);
    });

AX_TEST_F(
    'DictationMV2MacrosTest', 'RepeatableKeyPressMacro', async function() {
      // DELETE_PREV_CHAR is one of many RepeatableKeyPressMacros.
      // Toggle Dictation on so that the Macro will be runnable.
      this.toggleDictationOn();
      const macro = await this.getSimpleParseStrategy().parse('delete');
      assertEquals('DELETE_PREV_CHAR', macro.getNameAsString());
      const checkContextResult = macro.checkContext();
      assertTrue(checkContextResult.canTryAction);
      assertEquals(undefined, checkContextResult.error);
      const runMacroResult = macro.run();
      assertTrue(runMacroResult.isSuccess);
      assertEquals(undefined, runMacroResult.error);
    });

// Disabled due to flaky test: crbug.com/413223744
AX_TEST_F('DictationMV2MacrosTest', 'DISABLED_ListCommandsMacro', async function() {
  this.toggleDictationOn();
  const macro = await this.getSimpleParseStrategy().parse('help');
  assertEquals('LIST_COMMANDS', macro.getNameAsString());
  const checkContextResult = macro.checkContext();
  assertTrue(checkContextResult.canTryAction);
  assertEquals(undefined, checkContextResult.error);
  const runMacroResult = macro.run();
  assertTrue(runMacroResult.isSuccess);
  assertEquals(undefined, runMacroResult.error);
});

AX_TEST_F('DictationMV2MacrosTest', 'StopListeningMacro', async function() {
  this.toggleDictationOn();
  assertTrue(this.getDictationActive());
  assertTrue(this.getSpeechRecognitionActive());
  const macro = new ToggleDictationMacro();
  assertEquals('TOGGLE_DICTATION', macro.getNameAsString());
  const checkContextResult = macro.checkContext();
  assertTrue(checkContextResult.canTryAction);
  assertEquals(undefined, checkContextResult.error);
  const runMacroResult = macro.run();
  assertTrue(runMacroResult.isSuccess);
  assertEquals(undefined, runMacroResult.error);
  assertFalse(this.getDictationActive());
  assertFalse(this.getSpeechRecognitionActive());
});

// Tests that smart macros can be parsed and constructed with the correct
// arguments.
SYNC_TEST_F('DictationMV2MacrosTest', 'SmartMacros', async function() {
  const strategy = this.getSimpleParseStrategy();
  assertNotNullNorUndefined(strategy);
  let macro = await strategy.parse('delete hello world');
  assertEquals('SMART_DELETE_PHRASE', macro.getNameAsString());
  assertEquals('hello world', macro.phrase_);
  macro = await strategy.parse('replace hello with goodbye');
  assertEquals('SMART_REPLACE_PHRASE', macro.getNameAsString());
  assertEquals('hello', macro.deletePhrase_);
  assertEquals('goodbye', macro.insertPhrase_);
  macro = await strategy.parse('insert hello before goodbye');
  assertEquals('SMART_INSERT_BEFORE', macro.getNameAsString());
  assertEquals('hello', macro.insertPhrase_);
  assertEquals('goodbye', macro.beforePhrase_);
  macro = await strategy.parse('select from hello to goodbye');
  assertEquals('SMART_SELECT_BTWN_INCL', macro.getNameAsString());
  assertEquals('hello', macro.startPhrase_);
  assertEquals('goodbye', macro.endPhrase_);
});

AX_TEST_F(
    'DictationMV2MacrosTest', 'UnselectInactiveInputController',
    async function() {
      const macro = new UnselectTextMacro(new InputControllerImpl());
      assertEquals('UNSELECT_TEXT', macro.getNameAsString());
      const contextResult = macro.checkContext();
      assertFalse(contextResult.canTryAction);
      assertEquals(MacroError.BAD_CONTEXT, contextResult.error);
      assertEquals(
          Context.INACTIVE_INPUT_CONTROLLER, contextResult.failedContext);
    });
