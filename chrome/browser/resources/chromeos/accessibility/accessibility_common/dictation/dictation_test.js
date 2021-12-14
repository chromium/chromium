// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['dictation_test_base.js']);

/**
 * Dictation feature using accessibility common extension browser tests.
 */
DictationE2ETest = class extends DictationE2ETestBase {
  constructor() {
    super();

    this.commandStrings = {
      DELETE_PREV_CHAR: 'delete',
      NAV_PREV_CHAR: 'move to the previous character',
      NAV_NEXT_CHAR: 'move to the next character',
      NAV_PREV_LINE: 'move to the previous line',
      NAV_NEXT_LINE: 'move to the next line',
      COPY_SELECTED_TEXT: 'copy',
      PASTE_TEXT: 'paste',
      CUT_SELECTED_TEXT: 'cut',
      UNDO_TEXT_EDIT: 'undo',
      REDO_ACTION: 'redo',
      SELECT_ALL_TEXT: 'select all',
      UNSELECT_TEXT: 'unselect',
      LIST_COMMANDS: 'help',
      NEW_LINE: 'new line',
    };
  }
};

SYNC_TEST_F('DictationE2ETest', 'SanityCheck', async function() {
  await this.waitForDictationModule();
  assertFalse(this.mockAccessibilityPrivate.getDictationActive());
});

SYNC_TEST_F(
    'DictationE2ETest', 'LoadsAndUnloadsIMEAndSpeechRecognition',
    async function() {
      await this.waitForDictationModule();
      this.checkDictationImeInactive();
      this.toggleDictationOn(1);
      this.toggleDictationOffFromA11yPrivate();
    });

SYNC_TEST_F(
    'DictationE2ETest', 'TogglesDictationOffWhenIMEBlur', async function() {
      await this.waitForDictationModule();
      this.checkDictationImeInactive();
      this.toggleDictationOn(1);

      // Blur the input context. Dictation should get toggled off.
      this.mockInputIme.callOnBlur(1);
      assertFalse(this.mockAccessibilityPrivate.getDictationActive());
      // Speech recognition remains active until Dictation is toggled off.
      assertTrue(this.mockSpeechRecognitionPrivate.isStarted());
      // Now that we've confirmed that Dictation JS tried to toggle Dictation,
      // via AccessibilityPrivate, we can call the onToggleDictation
      // callback as AccessibilityManager would do, to allow Dictation JS to
      // clean up state.
      this.toggleDictationOffFromA11yPrivate();
      assertFalse(this.mockSpeechRecognitionPrivate.isStarted());
    });

SYNC_TEST_F(
    'DictationE2ETest', 'ResetsPreviousIMEAfterDeactivate', async function() {
      await this.waitForDictationModule();
      // Set something as the active IME.
      this.mockInputMethodPrivate.setCurrentInputMethod('keyboard_cat');
      this.mockLanguageSettingsPrivate.addInputMethod('keyboard_cat');

      this.toggleDictationOn(2);

      // Deactivate Dictation.
      this.mockAccessibilityPrivate.callOnToggleDictation(false);
      this.checkDictationImeInactive('keyboard_cat');
    });

SYNC_TEST_F('DictationE2ETest', 'SetsUpSpeechRecognition', async function() {
  await this.waitForDictationModule();
  // Speech Recognition should be ready but not started yet.
  assertFalse(this.mockSpeechRecognitionPrivate.isStarted());
  assertEquals(undefined, this.mockSpeechRecognitionPrivate.locale());
  assertEquals(undefined, this.mockSpeechRecognitionPrivate.interimResults());

  const locale = await this.getPref(Dictation.DICTATION_LOCALE_PREF);
  this.mockSpeechRecognitionPrivate.updateProperties(
      {locale: locale.value, interimResults: true});
  assertEquals(locale.value, this.mockSpeechRecognitionPrivate.locale());
  assertTrue(this.mockSpeechRecognitionPrivate.interimResults());
});

SYNC_TEST_F(
    'DictationE2ETest', 'ChangesSpeechRecognitionLangOnLocaleChange',
    async function() {
      await this.waitForDictationModule();
      let locale = await this.getPref(Dictation.DICTATION_LOCALE_PREF);
      this.mockSpeechRecognitionPrivate.updateProperties(
          {locale: locale.value});
      assertEquals(locale.value, this.mockSpeechRecognitionPrivate.locale());
      // Change the locale.
      await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'es-ES');
      // Wait for the callbacks to Dictation.
      locale = await this.getPref(Dictation.DICTATION_LOCALE_PREF);
      this.mockSpeechRecognitionPrivate.updateProperties(
          {locale: locale.value});
      assertEquals('es-ES', this.mockSpeechRecognitionPrivate.locale());
    });

SYNC_TEST_F(
    'DictationE2ETest', 'InterimResultsDependOnChromeVoxEnabled',
    async function() {
      await this.waitForDictationModule();
      await this.toggleDictationAndStartListening(4);

      // Interim results shown in composition.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent('one', false);
      this.assertImeCompositionParameters('one', 4);
      this.mockInputIme.clearLastParameters();

      // Toggle ChromeVox on.
      await this.setPref(Dictation.SPOKEN_FEEDBACK_PREF, true);
      await this.getPref(Dictation.SPOKEN_FEEDBACK_PREF);

      // Composition is not changed.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent('two', false);
      assertFalse(!!this.mockInputIme.getLastCompositionParameters());

      // Toggle ChromeVox off.
      await this.setPref(Dictation.SPOKEN_FEEDBACK_PREF, false);
      await this.getPref(Dictation.SPOKEN_FEEDBACK_PREF);

      // Interim results impact the composition again.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent('three', false);
      this.assertImeCompositionParameters('three', 4);
      this.mockInputIme.clearLastParameters();
    });

SYNC_TEST_F(
    'DictationE2ETest', 'StopsDictationOnSpeechRecognitionError',
    async function() {
      await this.waitForDictationModule();
      this.toggleDictationOn(1);

      // An error is received.
      this.mockSpeechRecognitionPrivate.fireMockOnErrorEvent();

      // Check that a request to toggle dictation off was sent and that speech
      // recognition has stopped.
      assertFalse(this.mockAccessibilityPrivate.getDictationActive());
      assertFalse(this.mockSpeechRecognitionPrivate.isStarted());
    });

SYNC_TEST_F(
    'DictationE2ETest', 'StopsDictationOnIMEDeactivate', async function() {
      await this.waitForDictationModule();
      this.toggleDictationOn(1);

      // Focus and blur an input context to cancel Dictation.
      this.mockInputIme.callOnFocus(1);
      this.mockInputIme.callOnBlur(1);

      // Speech recognition remains active until Dictation is toggled off.
      assertTrue(this.mockSpeechRecognitionPrivate.isStarted());
      // Check that a request to toggle dictation off was sent.
      assertFalse(this.mockAccessibilityPrivate.getDictationActive());

      // Speech recognition is active, although shutdown is already in
      // progress.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent('kitties', true);

      // Complete toggle -- this is async so other things could have
      // happened in the meantime.
      this.mockAccessibilityPrivate.callOnToggleDictation(false);
      assertFalse(this.mockSpeechRecognitionPrivate.isStarted());

      // Nothing was committed.
      assertFalse(!!this.mockInputIme.getLastCompositionParameters());
      assertFalse(!!this.mockInputIme.getLastCommittedParameters());
    });

SYNC_TEST_F('DictationE2ETest', 'CommitsFinalizedText', async function() {
  await this.toggleDictationAndStartListening(/*contextID=*/ 3);
  this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
      'kitties 4 eva', true);
  await this.assertImeCommitParameters('kitties 4 eva', 3);
  assertFalse(!!this.mockInputIme.getLastCompositionParameters());
  assertTrue(this.mockAccessibilityPrivate.getDictationActive());

  this.mockInputIme.clearLastParameters();
  this.mockAccessibilityPrivate.callOnToggleDictation(false);
  assertFalse(!!this.mockInputIme.getLastCommittedParameters());
});

SYNC_TEST_F(
    'DictationE2ETest', 'CommitsMultipleResultsOfFinalizedText',
    async function() {
      await this.toggleDictationAndStartListening(5);
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent('kittens', false);
      this.assertImeCompositionParameters('kittens', 5);
      assertFalse(!!this.mockInputIme.getLastCommittedParameters());
      assertTrue(this.mockAccessibilityPrivate.getDictationActive());

      this.mockInputIme.clearLastParameters();
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent('kittens!', true);
      await this.assertImeCommitParameters('kittens!', 5);
      assertFalse(!!this.mockInputIme.getLastCompositionParameters());
      assertTrue(this.mockAccessibilityPrivate.getDictationActive());

      this.mockInputIme.clearLastParameters();
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent('puppies!', true);
      await this.assertImeCommitParameters('puppies!', 5);
      assertFalse(!!this.mockInputIme.getLastCompositionParameters());
      assertTrue(this.mockAccessibilityPrivate.getDictationActive());
    });

SYNC_TEST_F(
    'DictationE2ETest', 'SetsCompositionForInterimResults', async function() {
      await this.toggleDictationAndStartListening(2);

      // Send an interim result.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
          'dogs drool', false);
      this.assertImeCompositionParameters('dogs drool', 2);
      assertFalse(!!this.mockInputIme.getLastCommittedParameters());
    });

SYNC_TEST_F(
    'DictationE2ETest', 'CommitsCompositionOnToggleOff', async function() {
      await this.toggleDictationAndStartListening(1);

      // Send some interim result.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
          'fish fly', false);
      this.assertImeCompositionParameters('fish fly', 1);
      this.mockInputIme.clearLastParameters();

      // Dictation toggles off after speech recognition sends a stop
      // event.
      this.mockSpeechRecognitionPrivate.fireMockStopEvent();
      assertFalse(this.mockAccessibilityPrivate.getDictationActive());
      this.mockAccessibilityPrivate.callOnToggleDictation(false);

      // The interim result should have been committed.
      await this.assertImeCommitParameters('fish fly', 1);
      assertFalse(!!this.mockInputIme.getLastCompositionParameters());
    });

SYNC_TEST_F(
    'DictationE2ETest', 'DoesNotCommitCompositionAfterIMEBlur',
    async function() {
      await this.toggleDictationAndStartListening(4);

      // Send some interim result.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
          'ducks dig', false);
      this.assertImeCompositionParameters('ducks dig', 4);

      // Dictation toggles off blur of the active context ID.
      this.mockInputIme.callOnBlur(4);
      assertFalse(this.mockAccessibilityPrivate.getDictationActive());
      this.mockAccessibilityPrivate.callOnToggleDictation(false);

      // The interim result should not have been committed.
      assertFalse(!!this.mockInputIme.getLastCommittedParameters());
    });

SYNC_TEST_F('DictationE2ETest', 'TimesOutWithNoIMEContext', async function() {
  this.mockSetTimeoutMethod();
  await this.waitForDictationModule();
  this.mockAccessibilityPrivate.callOnToggleDictation(true);

  assertFalse(this.mockSpeechRecognitionPrivate.isStarted());
  assertTrue(!!this.lastSetTimeoutCallback);
  assertEquals(this.lastSetDelay, Dictation.Timeouts.NO_FOCUSED_IME_MS);

  // Triggering the timeout should cause a request to toggle Dictation, but
  // nothing should be committed after AccessibilityPrivate toggle is received.
  this.lastSetTimeoutCallback();
  this.lastSetTimeoutCallback = null;
  assertFalse(this.mockAccessibilityPrivate.getDictationActive());
  this.mockAccessibilityPrivate.callOnToggleDictation(false);

  // Nothing was committed.
  assertFalse(!!this.mockInputIme.getLastCommittedParameters());
  assertFalse(!!this.mockInputIme.getLastCompositionParameters());
});

SYNC_TEST_F('DictationE2ETest', 'TimesOutWithNoSpeech', async function() {
  this.mockSetTimeoutMethod();
  await this.waitForDictationModule();
  this.mockAccessibilityPrivate.callOnToggleDictation(true);
  this.mockInputIme.callOnFocus(1);

  assertTrue(!!this.lastSetTimeoutCallback);
  assertEquals(this.lastSetDelay, Dictation.Timeouts.NO_SPEECH_MS);

  // Triggering the timeout should cause a request to toggle Dictation, but
  // nothing should be committed after AccessibilityPrivate toggle is received.
  this.lastSetTimeoutCallback();
  this.lastSetTimeoutCallback = null;
  assertFalse(this.mockAccessibilityPrivate.getDictationActive());
  this.mockAccessibilityPrivate.callOnToggleDictation(false);

  // Nothing was committed.
  assertFalse(!!this.mockInputIme.getLastCommittedParameters());
  assertFalse(!!this.mockInputIme.getLastCompositionParameters());
});

SYNC_TEST_F(
    'DictationE2ETest', 'TimesOutAfterInterimResultsAndCommits',
    async function() {
      this.mockSetTimeoutMethod();
      await this.toggleDictationAndStartListening(6);
      // Send some interim result.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
          'sheep sleep', false);
      this.assertImeCompositionParameters('sheep sleep', 6);
      this.mockInputIme.clearLastParameters();

      // The timeout should be set based on the interim result.
      assertTrue(!!this.lastSetTimeoutCallback);
      assertEquals(this.lastSetDelay, Dictation.Timeouts.NO_NEW_SPEECH_MS);

      // Triggering the timeout should cause a request to toggle Dictation, and
      // after AccessibilityPrivate calls toggleDictation, to commit the interim
      // text.
      this.lastSetTimeoutCallback();
      this.lastSetTimeoutCallback = null;
      assertFalse(this.mockAccessibilityPrivate.getDictationActive());
      this.mockAccessibilityPrivate.callOnToggleDictation(false);
      await this.assertImeCommitParameters('sheep sleep', 6);
      assertFalse(!!this.mockInputIme.getLastCompositionParameters());
    });

SYNC_TEST_F('DictationE2ETest', 'TimesOutAfterFinalResults', async function() {
  this.mockSetTimeoutMethod();
  await this.toggleDictationAndStartListening(7);
  // Send some final result.
  this.mockSpeechRecognitionPrivate.fireMockOnResultEvent('bats bounce', true);
  await this.assertImeCommitParameters('bats bounce', 7);
  this.mockInputIme.clearLastParameters();

  // The timeout should be set based on the final result.
  assertTrue(!!this.lastSetTimeoutCallback);
  assertEquals(this.lastSetDelay, Dictation.Timeouts.NO_SPEECH_MS);

  // Triggering the timeout should stop listening.
  this.lastSetTimeoutCallback();
  this.lastSetTimeoutCallback = null;
  assertFalse(this.mockAccessibilityPrivate.getDictationActive());
  this.mockAccessibilityPrivate.callOnToggleDictation(false);

  // Nothing new was committed.
  assertFalse(!!this.mockInputIme.getLastCommittedParameters());
  assertFalse(!!this.mockInputIme.getLastCompositionParameters());
});

SYNC_TEST_F(
    'DictationE2ETest', 'CommandsCommitWithoutFlagEnabled', async function() {
      await this.toggleDictationAndStartListening(8);
      for (const command of Object.values(this.commandStrings)) {
        this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(command, false);
        this.assertImeCompositionParameters(command, 8);

        // On final result, composition is committed as usual.
        this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(command, true);
        await this.assertImeCommitParameters(command, 8);

        this.mockInputIme.clearLastParameters();
      }
    });

SYNC_TEST_F(
    'DictationE2ETest', 'CommandsDoNotCommitThemselves', async function() {
      await this.waitForDictationModule();
      await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'en-US');
      await this.setCommandsEnabledForTest(true);
      await this.toggleDictationAndStartListening(8);
      for (const command of Object.values(this.commandStrings)) {
        this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(command, false);
        // Nothing is added to composition text when commands UI is enabled.
        assertFalse(!!this.mockInputIme.getLastCompositionParameters());
        // TODO(crbug.com/1252037): Check UI shows correct command info.

        if (command !== this.commandStrings.LIST_COMMANDS) {
          // LIST_COMMANDS opens a new tab and ends Dictation. Skip this.
          this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
              command, true);
        }
        if (command === this.commandStrings.NEW_LINE) {
          await this.assertImeCommitParameters('\n', 8);
          this.mockInputIme.clearLastParameters();
        } else {
          // On final result, composition is cleared, nothing is committed
          // (instead, an action is taken).
          assertFalse(!!this.mockInputIme.getLastCommittedParameters());
        }

        // Try to type the command e.g. "type delete".
        this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
            'type ' + command, true);
        // The command should be entered but not the word "type".
        await this.assertImeCommitParameters(command, 8);

        this.mockInputIme.clearLastParameters();
      }
    });
