// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/mock_accessibility_private.js']);
GEN_INCLUDE(['../../common/testing/mock_input_ime.js']);
GEN_INCLUDE(['../../common/testing/mock_input_method_private.js']);
GEN_INCLUDE(['../../common/testing/mock_language_settings_private.js']);
GEN_INCLUDE(['../../common/testing/mock_speech_recognition.js']);

/**
 * Dictation feature using accessibility common extension browser tests.
 */
DictationE2ETest = class extends E2ETestBase {
  constructor() {
    super();
    this.mockAccessibilityPrivate = MockAccessibilityPrivate;
    chrome.accessibilityPrivate = this.mockAccessibilityPrivate;

    this.mockInputIme = MockInputIme;
    chrome.input.ime = this.mockInputIme;

    this.mockInputMethodPrivate = MockInputMethodPrivate;
    chrome.inputMethodPrivate = this.mockInputMethodPrivate;

    this.mockLanguageSettingsPrivate = MockLanguageSettingsPrivate;
    chrome.languageSettingsPrivate = this.mockLanguageSettingsPrivate;

    window.mockSpeechRecognition = new MockSpeechRecognition();
    window.webkitSpeechRecognition = MockSpeechRecognizer;

    this.dictationEngineId =
        '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

    this.commandStrings = [
      'delete', 'move left', 'move right', 'move up', 'move down', 'copy',
      'paste', 'cut', 'undo', 'redo', 'select all', 'unselect all', 'new line'
    ];

    this.lastSetTimeoutCallback = null;
    this.lastSetDelay = -1;

    // Re-initialize AccessibilityCommon with mock APIs.
    const reinit = module => {
      accessibilityCommon = new module.AccessibilityCommon();
    };
import('/accessibility_common/accessibility_common_loader.js').then(reinit);
  }

  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
    ::switches::kEnableExperimentalAccessibilityDictationExtension);
  base::OnceClosure load_cb =
    base::BindOnce(&ash::AccessibilityManager::SetDictationEnabled,
        base::Unretained(ash::AccessibilityManager::Get()),
        true);
    `);
    super.testGenPreambleCommon('kAccessibilityCommonExtensionId');
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kExperimentalAccessibilityDictationCommands']};
  }

  /**
   * Waits for Dictation module to be loaded.
   */
  async waitForDictationModule() {
    await importModule(
        'Dictation', '/accessibility_common/dictation/dictation.js');
    assertNotNullNorUndefined(Dictation);
    // Enable Dictation.
    await new Promise(resolve => {
      chrome.accessibilityFeatures.dictation.set({value: true}, resolve);
    });
    return new Promise(resolve => {
      resolve();
    });
  }

  /**
   * Async function to get a preference value from Settings.
   * @param {string} name
   */
  async getPref(name) {
    return new Promise(resolve => {
      chrome.settingsPrivate.getPref(name, (ret) => {
        resolve(ret);
      });
    });
  }

  /**
   * Async function to set a preference value in Settings.
   * @param {string} name
   */
  async setPref(name, value) {
    return new Promise(resolve => {
      chrome.settingsPrivate.setPref(name, value, undefined, () => {
        resolve();
      });
    });
  }

  /** Checks that Dictation is the active IME. */
  checkDictationImeActive() {
    assertEquals(
        this.dictationEngineId,
        this.mockInputMethodPrivate.getCurrentInputMethodForTest());
    assertTrue(this.mockLanguageSettingsPrivate.hasInputMethod(
        this.dictationEngineId));
  }

  /*
   * Checks that Dictation is not the active IME.
   * @param {*} opt_activeImeId If we do not expect Dictation IME to be
   *     activated, an optional IME ID that we do expect to be activated.
   */
  checkDictationImeInactive(opt_activeImeId) {
    assertNotEquals(
        this.dictationEngineId,
        this.mockInputMethodPrivate.getCurrentInputMethodForTest());
    assertFalse(this.mockLanguageSettingsPrivate.hasInputMethod(
        this.dictationEngineId));
    if (opt_activeImeId) {
      assertEquals(
          opt_activeImeId,
          this.mockInputMethodPrivate.getCurrentInputMethodForTest());
    }
  }

  /** Turns on Dictation and checks IME and Speech Recognition state. */
  toggleDictationOn(contextId) {
    this.mockAccessibilityPrivate.callOnToggleDictation(true);
    assertTrue(this.mockAccessibilityPrivate.getDictationActive());
    this.checkDictationImeActive();
    this.mockInputIme.callOnFocus(contextId);
    assertTrue(mockSpeechRecognition.isStarted());
  }

  /**
   * Turns Dictation off from AccessibilityPrivate and checks IME and Speech
   * Recognition state. Note that Dictation can also be toggled off by blurring
   * the current input context, SR errors, or timeouts.
   */
  toggleDictationOffFromA11yPrivate() {
    this.mockAccessibilityPrivate.callOnToggleDictation(false);
    assertFalse(this.mockAccessibilityPrivate.getDictationActive());
    this.checkDictationImeInactive();
    assertFalse(mockSpeechRecognition.isStarted());
  }

  /**
   * Waits for the Dictation module, starts Dictation from AccessibilityPrivate,
   * focuses the given |contextID|, then starts Speech Recognition.
   * @param {number} contextID
   */
  async toggleDictationAndStartListening(contextID) {
    await this.waitForDictationModule();
    this.mockAccessibilityPrivate.callOnToggleDictation(true);
    this.mockInputIme.callOnFocus(contextID);
    mockSpeechRecognition.callOnStart();
  }

  mockSetTimeoutMethod() {
    setTimeout = (callback, delay) => {
      this.lastSetTimeoutCallback = callback;
      this.lastSetDelay = delay;
    };
  }

  setCommandsEnabledForTest(enabled) {
    this.mockAccessibilityPrivate.enableFeatureForTest(
        this.mockAccessibilityPrivate.AccessibilityFeature.DICTATION_COMMANDS,
        enabled);
    accessibilityCommon.dictation_.initialize_();
  }

  /**
   * Checks that the latest IME composition parameters match the expected
   * values.
   * @param {string} text
   * @param {number} contextID
   */
  assertImeCompositionParameters(text, contextID) {
    assertEquals(text, this.mockInputIme.getLastCompositionParameters().text);
    assertEquals(
        contextID, this.mockInputIme.getLastCompositionParameters().contextID);
  }

  /**
   * Checks that the latest IME commit parameters match the expected
   * values.
   * @param {string} text
   * @param {number} contextID
   */
  assertImeCommitParameters(text, contextID) {
    assertEquals(text, this.mockInputIme.getLastCommittedParameters().text);
    assertEquals(
        contextID, this.mockInputIme.getLastCommittedParameters().contextID);
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

      // Now that we've confirmed that Dictation JS tried to toggle Dictation,
      // via AccessibilityPrivate, we can call the onToggleDictation
      // callback as AccessibilityManager would do, to allow Dictation JS to
      // clean up state.
      this.toggleDictationOffFromA11yPrivate();
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
  assertFalse(mockSpeechRecognition.isStarted());
  assertTrue(mockSpeechRecognition.continuous());
  assertTrue(mockSpeechRecognition.interimResults());

  const locale = await this.getPref(Dictation.DICTATION_LOCALE_PREF);
  assertEquals(locale.value, mockSpeechRecognition.lang());
});

SYNC_TEST_F(
    'DictationE2ETest', 'ChangesSpeechRecognitionLangOnLocaleChange',
    async function() {
      await this.waitForDictationModule();
      const locale = await this.getPref(Dictation.DICTATION_LOCALE_PREF);
      assertEquals(locale.value, mockSpeechRecognition.lang());
      // Change the locale.
      await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'es-ES');
      // Wait for the callbacks to Dictation.
      await this.getPref(Dictation.DICTATION_LOCALE_PREF);
      assertEquals('es-ES', mockSpeechRecognition.lang());
    });

SYNC_TEST_F(
    'DictationE2ETest', 'InterimResultsDependOnChromeVoxEnabled',
    async function() {
      await this.waitForDictationModule();
      await this.toggleDictationAndStartListening(4);

      // Interim results shown in composition.
      mockSpeechRecognition.callOnResult('one', false);
      this.assertImeCompositionParameters('one', 4);
      this.mockInputIme.clearLastParameters();

      // Toggle ChromeVox on.
      await this.setPref(Dictation.SPOKEN_FEEDBACK_PREF, true);
      await this.getPref(Dictation.SPOKEN_FEEDBACK_PREF);

      // Composition is not changed.
      mockSpeechRecognition.callOnResult('two', false);
      assertFalse(!!this.mockInputIme.getLastCompositionParameters());

      // Toggle ChromeVox off.
      await this.setPref(Dictation.SPOKEN_FEEDBACK_PREF, false);
      await this.getPref(Dictation.SPOKEN_FEEDBACK_PREF);

      // Interim results impact the composition again.
      mockSpeechRecognition.callOnResult('three', false);
      this.assertImeCompositionParameters('three', 4);
      this.mockInputIme.clearLastParameters();

    });

SYNC_TEST_F(
    'DictationE2ETest', 'StopsDictationOnSpeechRecognitionError',
    async function() {
      await this.waitForDictationModule();
      this.toggleDictationOn(1);

      // Complete start-up.
      mockSpeechRecognition.callOnStart();

      // An error is received.
      mockSpeechRecognition.callOnError({});

      // Check that a request to toggle dictation off was sent.
      assertFalse(this.mockAccessibilityPrivate.getDictationActive());
      // Although Dictation was requested to be turned off, speech
      // recognition doesn't stop until toggle dictation is received from
      // AccessibilityPrivate.
      assertTrue(mockSpeechRecognition.isStarted());

      this.mockAccessibilityPrivate.callOnToggleDictation(false);
    });

SYNC_TEST_F(
    'DictationE2ETest', 'StopsDictationOnIMEDeactivateBeforeOnStartIsCalled',
    async function() {
      await this.waitForDictationModule();
      this.toggleDictationOn(1);

      // Focus and blur an input context to cancel Dictation.
      this.mockInputIme.callOnFocus(1);
      this.mockInputIme.callOnBlur(1);

      // Check that a request to toggle dictation off was sent.
      assertFalse(this.mockAccessibilityPrivate.getDictationActive());
      // Although Dictation was requested to be turned off, speech
      // recognition doesn't stop until toggle dictation is received from
      // AccessibilityPrivate.
      assertTrue(mockSpeechRecognition.isStarted());

      // Speech recognition is ready although shutdown is already in
      // progress.
      mockSpeechRecognition.callOnStart();
      mockSpeechRecognition.callOnResult('kitties', true);

      // Complete toggle -- this is async so other things could have
      // happened in the meantime.
      this.mockAccessibilityPrivate.callOnToggleDictation(false);

      // Nothing was committed.
      assertFalse(!!this.mockInputIme.getLastCompositionParameters());
      assertFalse(!!this.mockInputIme.getLastCommittedParameters());
    });

SYNC_TEST_F('DictationE2ETest', 'CommitsFinalizedText', async function() {
  await this.toggleDictationAndStartListening(/*contextID=*/ 3);
  mockSpeechRecognition.callOnResult('kitties 4 eva', true);
  this.assertImeCommitParameters('kitties 4 eva', 3);
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
      mockSpeechRecognition.callOnResult('kittens', false);
      this.assertImeCompositionParameters('kittens', 5);
      assertFalse(!!this.mockInputIme.getLastCommittedParameters());
      assertTrue(this.mockAccessibilityPrivate.getDictationActive());

      this.mockInputIme.clearLastParameters();
      mockSpeechRecognition.callOnResult('kittens!', true);
      this.assertImeCommitParameters('kittens!', 5);
      assertFalse(!!this.mockInputIme.getLastCompositionParameters());
      assertTrue(this.mockAccessibilityPrivate.getDictationActive());

      this.mockInputIme.clearLastParameters();
      mockSpeechRecognition.callOnResult('puppies!', true);
      this.assertImeCommitParameters('puppies!', 5);
      assertFalse(!!this.mockInputIme.getLastCompositionParameters());
      assertTrue(this.mockAccessibilityPrivate.getDictationActive());
    });

SYNC_TEST_F(
    'DictationE2ETest', 'SetsCompositionForInterimResults', async function() {
      await this.toggleDictationAndStartListening(2);

      // Send an interim result.
      mockSpeechRecognition.callOnResult('dogs drool', false);
      this.assertImeCompositionParameters('dogs drool', 2);
      assertFalse(!!this.mockInputIme.getLastCommittedParameters());
    });

SYNC_TEST_F(
    'DictationE2ETest', 'CommitsCompositionOnToggleOff', async function() {
      await this.toggleDictationAndStartListening(1);

      // Send some interim result.
      mockSpeechRecognition.callOnResult('fish fly', false);
      this.assertImeCompositionParameters('fish fly', 1);
      this.mockInputIme.clearLastParameters();

      // Dictation toggles off after speech recognition sends a stop
      // event.
      mockSpeechRecognition.callOnEnd();
      assertFalse(this.mockAccessibilityPrivate.getDictationActive());
      this.mockAccessibilityPrivate.callOnToggleDictation(false);

      // The interim result should have been committed.
      this.assertImeCommitParameters('fish fly', 1);
      assertFalse(!!this.mockInputIme.getLastCompositionParameters());
    });

SYNC_TEST_F(
    'DictationE2ETest', 'DoesNotCommitCompositionAfterIMEBlur',
    async function() {
      await this.toggleDictationAndStartListening(4);

      // Send some interim result.
      mockSpeechRecognition.callOnResult('ducks dig', false);
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
      mockSpeechRecognition.callOnResult('sheep sleep', false);
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
      this.assertImeCommitParameters('sheep sleep', 6);
      assertFalse(!!this.mockInputIme.getLastCompositionParameters());
    });

SYNC_TEST_F('DictationE2ETest', 'TimesOutAfterFinalResults', async function() {
  this.mockSetTimeoutMethod();
  await this.toggleDictationAndStartListening(7);
  // Send some final result.
  mockSpeechRecognition.callOnResult('bats bounce', true);
  this.assertImeCommitParameters('bats bounce', 7);
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
      for (const command of this.commandStrings) {
        mockSpeechRecognition.callOnResult(command, false);
        this.assertImeCompositionParameters(command, 8);

        // On final result, composition is committed as usual.
        mockSpeechRecognition.callOnResult(command, true);
        this.assertImeCommitParameters(command, 8);
      }
    });

SYNC_TEST_F(
    'DictationE2ETest', 'CommandsDoNotCommitThemselves', async function() {
      this.setCommandsEnabledForTest(true);
      await this.toggleDictationAndStartListening(8);
      for (const command of this.commandStrings) {
        mockSpeechRecognition.callOnResult(command, false);
        // Nothing is added to composition text when commands UI is enabled.
        assertFalse(!!this.mockInputIme.getLastCompositionParameters());
        // TODO(crbug.com/1252037): Check UI shows correct command info.

        mockSpeechRecognition.callOnResult(command, true);
        if (command === 'new line') {
          this.assertImeCommitParameters('\n', 8);
        } else {
          // On final result, composition is cleared, nothing is committed
          // (instead, an action is taken).
          assertFalse(!!this.mockInputIme.getLastCommittedParameters());
        }

        // Try a command to "type delete", etc.
        mockSpeechRecognition.callOnResult('type ' + command, true);
        // The command should be entered but not the word "type".
        this.assertImeCommitParameters(command, 8);

        this.mockInputIme.clearLastParameters();
      }
    });
