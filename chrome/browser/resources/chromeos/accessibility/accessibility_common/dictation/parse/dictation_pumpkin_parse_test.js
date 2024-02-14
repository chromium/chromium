// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../dictation_test_base.js']);

/**
 * Dictation tests for speech parsing with Pumpkin. These tests do not use the
 * live Pumpkin DLC, but instead use a local tar archive that mirrors the DLC.
 * It's important that we keep the live DLC and the local tar archive in sync.
 */
DictationPumpkinParseTest = class extends DictationE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await this.mockAccessibilityPrivate.initializePumpkinData();
    // Re-initialize PumpkinParseStrategy after mock Pumpkin data has been
    // created.
    this.getPumpkinParseStrategy().init_();

    await super.setUpDeferred();

    // By default, Dictation JS tests use regex parsing. Enable Pumpkin for
    // this test suite.
    this.getPumpkinParseStrategy().setEnabled(true);
  }

  /**
   * @return {!Promise}
   * @param {string=} locale An optional locale. If the locale is provided,
   * this method will wait for Pumpkin to initialize in that locale. If the
   * locale isn't provided, then this method will wait for Pumpkin to
   * initialize in any locale.
   * @private
   */
  async waitForPumpkinParseStrategy_(locale) {
    const strategy = this.getPumpkinParseStrategy();
    const isReady = () => {
      let localeOk = true;
      if (locale) {
        const pumpkinLocale = SUPPORTED_LOCALES[locale] || null;
        localeOk = pumpkinLocale === strategy.locale_;
      }

      return localeOk && strategy.pumpkinTaggerReady_;
    };

    if (isReady()) {
      return;
    }

    await new Promise(resolve => {
      strategy.onPumpkinTaggerReadyChangedForTesting_ = () => {
        if (isReady()) {
          strategy.onPumpkinTaggerReadyChangedForTesting_ = null;
          resolve();
        }
      };
    });
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
    new ParseTestCase('Hello world', {}),
    new ParseTestCase('dictate delete', {name: 'INPUT_TEXT_VIEW'}),
    new ParseTestCase('backspace', {name: 'DELETE_PREV_CHAR'}),
    new ParseTestCase('left one character', {name: 'NAV_PREV_CHAR'}),
    new ParseTestCase('right one character', {name: 'NAV_NEXT_CHAR'}),
    new ParseTestCase('up one line', {name: 'NAV_PREV_LINE'}),
    new ParseTestCase('down one line', {name: 'NAV_NEXT_LINE'}),
    new ParseTestCase('copy selected text', {name: 'COPY_SELECTED_TEXT'}),
    new ParseTestCase('paste copied text', {name: 'PASTE_TEXT'}),
    new ParseTestCase('cut highlighted text', {name: 'CUT_SELECTED_TEXT'}),
    new ParseTestCase('undo that', {name: 'UNDO_TEXT_EDIT'}),
    new ParseTestCase('redo that', {name: 'REDO_ACTION'}),
    new ParseTestCase('select everything', {name: 'SELECT_ALL_TEXT'}),
    new ParseTestCase('deselect selection', {name: 'UNSELECT_TEXT'}),
    new ParseTestCase('what can I say', {name: 'LIST_COMMANDS'}),
    new ParseTestCase('new line', {}),
    new ParseTestCase('avada kedavra', {name: 'TOGGLE_DICTATION'}),
    new ParseTestCase('clear one word', {name: 'DELETE_PREV_WORD'}),
    new ParseTestCase('erase sentence', {name: 'DELETE_PREV_SENT'}),
    new ParseTestCase('right one word', {name: 'NAV_NEXT_WORD'}),
    new ParseTestCase('back one word', {name: 'NAV_PREV_WORD'}),
    new ParseTestCase('delete avada kedavra', {name: 'SMART_DELETE_PHRASE'}),
    new ParseTestCase(
        'replace hello with goodbye', {name: 'SMART_REPLACE_PHRASE'}),
    new ParseTestCase(
        'insert hello in front of goodbye', {name: 'SMART_INSERT_BEFORE'}),
    new ParseTestCase(
        'highlight everything between hello and goodbye',
        {name: 'SMART_SELECT_BTWN_INCL'}),
    new ParseTestCase('forward one sentence', {name: 'NAV_NEXT_SENT'}),
    new ParseTestCase('one sentence back', {name: 'NAV_PREV_SENT'}),
    new ParseTestCase('clear', {name: 'DELETE_ALL_TEXT'}),
    new ParseTestCase('to start', {name: 'NAV_START_TEXT'}),
    new ParseTestCase('to end', {name: 'NAV_END_TEXT'}),
    new ParseTestCase('highlight back one word', {name: 'SELECT_PREV_WORD'}),
    new ParseTestCase('highlight right one word', {name: 'SELECT_NEXT_WORD'}),
    new ParseTestCase('select next letter', {name: 'SELECT_NEXT_CHAR'}),
    new ParseTestCase('select previous letter', {name: 'SELECT_PREV_CHAR'}),
    new ParseTestCase('try that action again', {name: 'REPEAT'}),
  ];

  for (const test of testCases) {
    await this.runPumpkinParseTestCase(test);
  }
});

// Tests that we can use the SandboxedPumpkinTagger to parse text and yield
// a RepeatableKeyPressMacro with a `repeat_` value greater than one.
AX_TEST_F(
    'DictationPumpkinParseTest', 'RepeatableKeyPressMacro', async function() {
      await this.waitForPumpkinParseStrategy_();

      /** @type {!Array<!ParseTestCase>} */
      const testCases = [
        new ParseTestCase(
            'remove two characters', {name: 'DELETE_PREV_CHAR', repeat: 2}),
        new ParseTestCase(
            'left five characters', {name: 'NAV_PREV_CHAR', repeat: 5}),
        new ParseTestCase(
            'clear five words', {name: 'DELETE_PREV_WORD', repeat: 5}),
        new ParseTestCase(
            'forward three words', {name: 'NAV_NEXT_WORD', repeat: 3}),
        new ParseTestCase(
            'backward three words', {name: 'NAV_PREV_WORD', repeat: 3}),
        new ParseTestCase(
            'highlight back three words',
            {name: 'SELECT_PREV_WORD', repeat: 3}),
        new ParseTestCase(
            'highlight right three words',
            {name: 'SELECT_NEXT_WORD', repeat: 3}),
        new ParseTestCase(
            'select next three letters', {name: 'SELECT_NEXT_CHAR', repeat: 3}),
        new ParseTestCase(
            'select previous three letters',
            {name: 'SELECT_PREV_CHAR', repeat: 3}),
      ];

      for (const test of testCases) {
        await this.runPumpkinParseTestCase(test);
      }
    });

// Tests that smart macro properties are correctly parsed and set.
AX_TEST_F('DictationPumpkinParseTest', 'SmartMacros', async function() {
  await this.waitForPumpkinParseStrategy_();

  let macro =
      await this.getPumpkinParseStrategy().parse('delete avada kedavra');
  assertEquals('SMART_DELETE_PHRASE', macro.getNameAsString());
  assertEquals('avada kedavra', macro.phrase_);

  macro =
      await this.getPumpkinParseStrategy().parse('replace hello with goodbye');
  assertEquals('SMART_REPLACE_PHRASE', macro.getNameAsString());
  assertEquals('hello', macro.deletePhrase_);
  assertEquals('goodbye', macro.insertPhrase_);

  macro = await this.getPumpkinParseStrategy().parse(
      'insert hello in front of goodbye');
  assertEquals('SMART_INSERT_BEFORE', macro.getNameAsString());
  assertEquals('hello', macro.insertPhrase_);
  assertEquals('goodbye', macro.beforePhrase_);

  macro = await this.getPumpkinParseStrategy().parse(
      'highlight everything between hello and goodbye');
  assertEquals('SMART_SELECT_BTWN_INCL', macro.getNameAsString());
  assertEquals('hello', macro.startPhrase_);
  assertEquals('goodbye', macro.endPhrase_);
});

AX_TEST_F('DictationPumpkinParseTest', 'ChangeLocale', async function() {
  await this.waitForPumpkinParseStrategy_();
  this.alwaysEnableCommands();
  const testCases = [
    {
      locale: 'fr-FR',
      testCase: new ParseTestCase('copier', {name: 'COPY_SELECTED_TEXT'}),
    },
    {
      locale: 'fr-FR',
      testCase: new ParseTestCase(
          'supprimer deux caractères précédent',
          {name: 'DELETE_PREV_CHAR', repeat: 2}),
    },
    {
      locale: 'it-IT',
      testCase: new ParseTestCase('annulla', {name: 'UNDO_TEXT_EDIT'}),
    },
    {
      locale: 'de-DE',
      testCase: new ParseTestCase('hilf mir', {name: 'LIST_COMMANDS'}),
    },
    {
      locale: 'es-ES',
      testCase: new ParseTestCase('ayuda', {name: 'LIST_COMMANDS'}),
    },
    {
      locale: 'en-GB',
      testCase:
          new ParseTestCase('copy selected text', {name: 'COPY_SELECTED_TEXT'}),
    },
  ];
  for (const {locale, testCase} of testCases) {
    await this.setPref(Dictation.DICTATION_LOCALE_PREF, locale);
    await this.waitForPumpkinParseStrategy_(locale);
    await this.runPumpkinParseTestCase(testCase);
  }
});

AX_TEST_F('DictationPumpkinParseTest', 'UnsupportedLocale', async function() {
  await this.waitForPumpkinParseStrategy_();
  this.alwaysEnableCommands();
  await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'ja');
  // Don't pass in a locale below because 'ja' is an unsupported locale (and
  // thus Pumpkin will never initialize in that locale).
  await this.waitForPumpkinParseStrategy_();
  await this.runPumpkinParseTestCase(
      new ParseTestCase('copy selected text', {}));
  // Would produce an UNDO_TEXT_EDIT macro if Japanese was supported.
  await this.runPumpkinParseTestCase(new ParseTestCase('もとどおりにする', {}));
  await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'en-US');
  await this.waitForPumpkinParseStrategy_('en-US');
  await this.runPumpkinParseTestCase(
      new ParseTestCase('copy selected text', {name: 'COPY_SELECTED_TEXT'}));
});
