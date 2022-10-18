// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../dictation_test_base.js']);

/** A class that represents a test case for parsing text. */
class ParseTestCase {
  /**
   * @param {string} text The text to be parsed
   * @param {string|undefined} expectedMacroName The expected name of the
   *     resulting macro.
   * @param {number|undefined} expectedRepeat The expected repeat value of the
   *     resulting macro.
   * @constructor
   */
  constructor(text, expectedMacroName, expectedRepeat) {
    /** @type {string} */
    this.text = text;
    /** @type {string|undefined} */
    this.expectedMacroName = expectedMacroName;
    /** @type {number|undefined} */
    this.expectedRepeat = expectedRepeat;
  }
}

/**
 * Dictation tests for speech parsing with Pumpkin. These tests do not use the
 * live Pumpkin DLC, but instead use a local tar archive that mirrors the DLC.
 * It's important that we keep the live DLC and the local tar archive in sync.
 * SandboxedPumpkinTagger emits several logs during the initialization
 * phase e.g. "Pumpkin module loaded". Setup this test so that it doesn't
 * fail when something is logged to the console.
 * TODO(https://crbug.com/1258190): Remove DictationE2ETestAllowConsole and
 * override the message filter so that wasm console messages don't cause the
 * test to fail.
 */
DictationPumpkinParseTest = class extends DictationE2ETestAllowConsole {
  /** @override */
  async setUpDeferred() {
    this.mockAccessibilityPrivate.enableFeatureForTest(
        'dictationPumpkinParsing', true);
    await this.mockAccessibilityPrivate.initializePumpkinData();
    // Re-initialize PumpkinParseStrategy after mock Pumpkin data has been
    // created.
    this.getPumpkinParseStrategy().init_();
    await importModule(
        'SpeechParser',
        '/accessibility_common/dictation/parse/speech_parser.js');

    await super.setUpDeferred();
  }

  /**
   * @return {!Promise}
   * @private
   */
  async waitForPumpkinParseStrategy_() {
    const strategy = this.getPumpkinParseStrategy();
    // TODO(crbug.com/1258190): Consider adding an observer or callback and
    // remove the polling below.
    return new Promise(resolve => {
      const intervalId = setInterval(() => {
        if (strategy.pumpkinTaggerReady_) {
          clearInterval(intervalId);
          resolve();
        }
      }, 300);
    });
  }

  /**
   * @param {!ParseTestCase} testCase
   * @return {!Promise}
   */
  async runParseTestCase(testCase) {
    const expectedMacroName = testCase.expectedMacroName;
    const expectedRepeat = testCase.expectedRepeat;
    const macro = await this.getPumpkinParseStrategy().parse(testCase.text);
    if (!macro) {
      assertEquals(undefined, expectedMacroName);
      assertEquals(undefined, expectedRepeat);
      return;
    }

    if (expectedMacroName) {
      assertEquals(expectedMacroName, macro.getMacroNameString());
    }
    if (expectedRepeat) {
      assertEquals(expectedRepeat, macro.repeat_);
    }
  }
};

// Tests that we can use the SandboxedPumpkinTagger to convert speech into a
// macro. The text to macro mapping can be found in
// google3/chrome/chromeos/accessibility/dictation/grammars/\
// dictation_en_us.patterns
AX_TEST_F('DictationPumpkinParseTest', 'Parse', async function() {
  await this.waitForPumpkinParseStrategy_();

  /** @type {!Array<!ParseTestCase>} */
  const testCases = [
    new ParseTestCase('Hello world'),
    new ParseTestCase('dictate delete', 'INPUT_TEXT_VIEW'),
    new ParseTestCase('backspace', 'DELETE_PREV_CHAR'),
    new ParseTestCase('left one character', 'NAV_PREV_CHAR'),
    new ParseTestCase('right one character', 'NAV_NEXT_CHAR'),
    new ParseTestCase('up one line', 'NAV_PREV_LINE'),
    new ParseTestCase('down one line', 'NAV_NEXT_LINE'),
    new ParseTestCase('copy selected text', 'COPY_SELECTED_TEXT'),
    new ParseTestCase('paste copied text', 'PASTE_TEXT'),
    new ParseTestCase('cut highlighted text', 'CUT_SELECTED_TEXT'),
    new ParseTestCase('undo that', 'UNDO_TEXT_EDIT'),
    new ParseTestCase('redo that', 'REDO_ACTION'),
    new ParseTestCase('select everything', 'SELECT_ALL_TEXT'),
    new ParseTestCase('deselect selection', 'UNSELECT_TEXT'),
    new ParseTestCase('what can I say', 'LIST_COMMANDS'),
    new ParseTestCase('new line'),
  ];

  for (const test of testCases) {
    await this.runParseTestCase(test);
  }
});

// Tests that we can use the SandboxedPumpkinTagger to parse text and yield
// a RepeatableKeyPressMacro with a `repeat_` value greater than one.
AX_TEST_F(
    'DictationPumpkinParseTest', 'RepeatableKeyPressMacro', async function() {
      await this.waitForPumpkinParseStrategy_();

      /** @type {!Array<!ParseTestCase>} */
      const testCases = [
        new ParseTestCase('remove two characters', 'DELETE_PREV_CHAR', 2),
        new ParseTestCase('left five characters', 'NAV_PREV_CHAR', 5),
      ];

      for (const test of testCases) {
        await this.runParseTestCase(test);
      }
    });

AX_TEST_F('DictationPumpkinParseTest', 'ChangeLocale', async function() {
  await this.waitForPumpkinParseStrategy_();
  this.alwaysEnableCommands();
  const testCases = [
    {
      locale: 'fr-FR',
      testCase: new ParseTestCase('copier', 'COPY_SELECTED_TEXT'),
    },
    {
      locale: 'fr-FR',
      testCase: new ParseTestCase(
          'supprimer deux caractères précédent', 'DELETE_PREV_CHAR', 2),
    },
    {locale: 'it-IT', testCase: new ParseTestCase('annulla', 'UNDO_TEXT_EDIT')},
    {locale: 'de-DE', testCase: new ParseTestCase('hilf mir', 'LIST_COMMANDS')},
    {locale: 'es-ES', testCase: new ParseTestCase('ayuda', 'LIST_COMMANDS')},
    {
      locale: 'en-GB',
      testCase: new ParseTestCase('copy selected text', 'COPY_SELECTED_TEXT'),
    },
  ];
  for (const {locale, testCase} of testCases) {
    await this.setPref(Dictation.DICTATION_LOCALE_PREF, locale);
    await this.waitForPumpkinParseStrategy_();
    await this.runParseTestCase(testCase);
  }
});

AX_TEST_F('DictationPumpkinParseTest', 'UnsupportedLocale', async function() {
  await this.waitForPumpkinParseStrategy_();
  this.alwaysEnableCommands();
  await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'ja');
  await this.waitForPumpkinParseStrategy_();
  await this.runParseTestCase(new ParseTestCase('copy selected text'));
  // Would produce an UNDO_TEXT_EDIT macro if Japanese was supported.
  await this.runParseTestCase(new ParseTestCase('もとどおりにする'));
  await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'en-US');
  await this.waitForPumpkinParseStrategy_();
  await this.runParseTestCase(
      new ParseTestCase('copy selected text', 'COPY_SELECTED_TEXT'));
});
