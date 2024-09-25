// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/mock_accessibility_private.js']);
GEN_INCLUDE(['../../common/testing/mock_audio.js']);
GEN_INCLUDE(['../../common/testing/mock_input_ime.js']);
GEN_INCLUDE(['../../common/testing/mock_input_method_private.js']);
GEN_INCLUDE(['../../common/testing/mock_language_settings_private.js']);
GEN_INCLUDE(['../../common/testing/mock_speech_recognition_private.js']);

/**
 * @typedef {{
 *   name: (string|undefined),
 *   repeat: (number|undefined),
 *   smart: (boolean|undefined),
 * }}
 */
let ParseTestExpectations;

/** A class that represents a test case for parsing text. */
class ParseTestCase {
  /**
   * @param {string} text The text to be parsed
   * @param {!ParseTestExpectations} expectations
   * @constructor
   */
  constructor(text, expectations) {
    /** @type {string} */
    this.text = text;
    /** @type {string|undefined} */
    this.expectedName = expectations.name;
    /** @type {number|undefined} */
    this.expectedRepeat = expectations.repeat;
    /** @type {boolean|undefined} */
    this.expectedSmart = expectations.smart;
  }
}

/**
 * Base class for tests for Dictation feature using accessibility common
 * extension browser tests.
 */
DictationE2ETestBase = class extends E2ETestBase {
  constructor() {
    super();

    this.mockAccessibilityPrivate = new MockAccessibilityPrivate();
    this.iconType = this.mockAccessibilityPrivate.DictationBubbleIconType;
    this.hintType = this.mockAccessibilityPrivate.DictationBubbleHintType;
    chrome.accessibilityPrivate = this.mockAccessibilityPrivate;

    this.mockInputIme = MockInputIme;
    chrome.input.ime = this.mockInputIme;

    this.mockInputMethodPrivate = MockInputMethodPrivate;
    chrome.inputMethodPrivate = this.mockInputMethodPrivate;

    this.mockLanguageSettingsPrivate = MockLanguageSettingsPrivate;
    chrome.languageSettingsPrivate = this.mockLanguageSettingsPrivate;

    this.mockSpeechRecognitionPrivate = new MockSpeechRecognitionPrivate();
    chrome.speechRecognitionPrivate = this.mockSpeechRecognitionPrivate;

    this.mockAudio = new MockAudio();
    chrome.audio = this.mockAudio;

    this.dictationEngineId =
        '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

    /** @private {!Array<Object{delay: number, callback: Function}} */
    this.setTimeoutData_ = [];

    /** @private {number} */
    this.imeContextId_ = 1;

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

    // Re-initialize AccessibilityCommon with mock APIs.
    accessibilityCommon = new AccessibilityCommon();
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Wait for the Dictation module to load and set the Dictation locale.
    await new Promise(
        resolve =>
            chrome.accessibilityFeatures.dictation.set({value: true}, resolve));
    assertNotNullNorUndefined(Dictation);
    await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'en-US');

    // By default, Dictation JS tests should use regex parsing.
    accessibilityCommon.dictation_.disablePumpkinForTesting();
    // Increase Dictation's NO_FOCUSED_IME timeout to reduce flakiness on slower
    // builds.
    accessibilityCommon.dictation_.setNoFocusedImeTimeoutForTesting(20 * 1000);
  }

  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/command_line.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "ui/accessibility/accessibility_features.h"
#include "components/prefs/pref_service.h"
#include "ash/constants/ash_pref_names.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();

    GEN(`
  GetProfile()->GetPrefs()->SetBoolean(
        ash::prefs::kDictationAcceleratorDialogHasBeenAccepted, true);

  base::OnceClosure load_cb =
    base::BindOnce(&ash::AccessibilityManager::SetDictationEnabled,
        base::Unretained(ash::AccessibilityManager::Get()),
        true);
    `);

    // Allow informational Pumpkin messages.
    super.testGenPreambleCommon(
        /*extensionIdName=*/ 'kAccessibilityCommonExtensionId',
        /*failOnConsoleError=*/ true,
        /*allowedMessages=*/[
          'Pumpkin installed, but data is empty',
          `wasm streaming compile failed: TypeError: Failed to execute ` +
              `'compile' on 'WebAssembly': Incorrect response MIME type. ` +
              `Expected 'application/wasm'.`,
          'falling back to ArrayBuffer instantiation',
          'Pumpkin module loaded.',
          `Unchecked runtime.lastError: Couldn't retrieve Pumpkin data.`,
        ]);
  }

  /** Turns on Dictation and checks IME and Speech Recognition state. */
  toggleDictationOn() {
    this.mockAccessibilityPrivate.callOnToggleDictation(true);
    assertTrue(this.getDictationActive());
    this.checkDictationImeActive();
    this.focusInputContext();
    assertTrue(this.getSpeechRecognitionActive());
  }

  /**
   * Turns Dictation off and checks IME and Speech Recognition state. Note that
   * Dictation can also be toggled off by blurring the current input context,
   * Speech recognition errors, or timeouts.
   */
  toggleDictationOff() {
    this.mockAccessibilityPrivate.callOnToggleDictation(false);
    assertFalse(
        this.getDictationActive(),
        'Dictation should be inactive after toggling Dictation');
    this.checkDictationImeInactive();
    assertFalse(
        this.getSpeechRecognitionActive(), 'Speech recognition should be off');
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

  // Timeout methods.

  mockSetTimeoutMethod() {
    setTimeout = (callback, delay) => {
      // setTimeout can be called from several different sources, so track
      // them using an Array.
      this.setTimeoutData_.push({delay, callback});
    };
  }

  /** @return {?Function} */
  getCallbackWithDelay(delay) {
    for (const data of this.setTimeoutData_) {
      if (data.delay === delay) {
        return data.callback;
      }
    }

    return null;
  }

  clearSetTimeoutData() {
    this.setTimeoutData_ = [];
  }

  // Ime methods.

  focusInputContext() {
    this.mockInputIme.callOnFocus(this.imeContextId_);
  }

  blurInputContext() {
    this.mockInputIme.callOnBlur(this.imeContextId_);
  }

  /**
   * Checks that the latest IME commit text matches the expected value.
   * @param {string} expected
   * @return {!Promise}
   */
  async assertCommittedText(expected) {
    if (!this.mockInputIme.getLastCommittedParameters()) {
      await this.mockInputIme.waitForCommit();
    }
    assertEquals(expected, this.mockInputIme.getLastCommittedParameters().text);
    assertEquals(
        this.imeContextId_,
        this.mockInputIme.getLastCommittedParameters().contextID);
  }

  // Getters and setters.

  /** @return {boolean} */
  getDictationActive() {
    return accessibilityCommon.dictation_.active_;
  }

  /** @return {InputTextStrategy} */
  getInputTextStrategy() {
    return accessibilityCommon.dictation_.speechParser_.inputTextStrategy_;
  }

  /** @return {SimpleParseStrategy} */
  getSimpleParseStrategy() {
    return accessibilityCommon.dictation_.speechParser_.simpleParseStrategy_;
  }

  /** @return {PumpkinParseStrategy} */
  getPumpkinParseStrategy() {
    return accessibilityCommon.dictation_.speechParser_.pumpkinParseStrategy_;
  }

  /** @return {InputController} */
  getInputController() {
    return accessibilityCommon.dictation_.inputController_;
  }

  // Speech recognition methods.

  /** @param {string} transcript */
  sendInterimSpeechResult(transcript) {
    this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
        transcript, /*is_final=*/ false);
  }

  /** @param {string} transcript */
  sendFinalSpeechResult(transcript) {
    this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
        transcript, /*is_final=*/ true);
  }

  sendSpeechRecognitionErrorEvent() {
    this.mockSpeechRecognitionPrivate.fireMockOnErrorEvent();
  }

  sendSpeechRecognitionStopEvent() {
    this.mockSpeechRecognitionPrivate.fireMockStopEvent();
  }

  /** @return {boolean} */
  getSpeechRecognitionActive() {
    return this.mockSpeechRecognitionPrivate.isStarted();
  }

  /** @return {string|undefined} */
  getSpeechRecognitionLocale() {
    return this.mockSpeechRecognitionPrivate.locale();
  }

  /** @return {boolean|undefined} */
  getSpeechRecognitionInterimResults() {
    return this.mockSpeechRecognitionPrivate.interimResults();
  }

  /**
   * @param {{
   *   clientId: (number|undefined),
   *   locale: (string|undefined),
   *   interimResults: (boolean|undefined)
   * }} properties
   */
  updateSpeechRecognitionProperties(properties) {
    this.mockSpeechRecognitionPrivate.updateProperties(properties);
  }

  // UI-related methods.

  /**
   * Waits for the updateDictationBubble() API to be called with the given
   * properties.
   * @param {DictationBubbleProperties} targetProps
   * @return {!Promise}
   */
  async waitForUIProperties(targetProps) {
    if (this.uiPropertiesMatch_(targetProps)) {
      return;
    }

    await new Promise(resolve => {
      const onUpdateDictationBubble = () => {
        if (this.uiPropertiesMatch_(targetProps)) {
          this.mockAccessibilityPrivate.removeUpdateDictationBubbleListener();
          resolve();
        }
      };

      this.mockAccessibilityPrivate.addUpdateDictationBubbleListener(
          onUpdateDictationBubble);
    });
  }

  /**
   * Returns true if `targetProps` matches the most recent UI properties. Must
   * match exactly.
   * @param {DictationBubbleProperties} targetProps
   * @return {boolean}
   * @private
   */
  uiPropertiesMatch_(targetProps) {
    /** @type {function(!Array<string>,!Array<string>) : boolean} */
    const areEqual = (arr1, arr2) => {
      return arr1.every((val, index) => val === arr2[index]);
    };

    const actualProps = this.mockAccessibilityPrivate.getDictationBubbleProps();
    if (!actualProps) {
      return false;
    }

    if (Object.keys(actualProps).length !== Object.keys(targetProps).length) {
      return false;
    }

    for (const key of Object.keys(targetProps)) {
      if (Array.isArray(targetProps[key]) && Array.isArray(actualProps[key])) {
        // For arrays, ensure that we compare the contents of the arrays.
        if (!areEqual(targetProps[key], actualProps[key])) {
          return false;
        }
      } else if (targetProps[key] !== actualProps[key]) {
        return false;
      }
    }

    return true;
  }

  /**
   * Always allows Dictation commands, even if the Dictation locale and browser
   * locale differ. Only used for testing.
   */
  alwaysEnableCommands() {
    LocaleInfo.alwaysEnableCommandsForTesting = true;
  }

  /**
   * @param {!ParseTestCase} testCase
   * @return {!Promise}
   */
  async runInputTextParseTestCase(testCase) {
    const macro = await this.getInputTextStrategy().parse(testCase.text);
    this.runParseTestCaseAssertions(testCase, macro);
  }

  /**
   * @param {!ParseTestCase} testCase
   * @return {!Promise}
   */
  async runSimpleParseTestCase(testCase) {
    const macro = await this.getSimpleParseStrategy().parse(testCase.text);
    this.runParseTestCaseAssertions(testCase, macro);
  }

  /**
   * @param {!ParseTestCase} testCase
   * @return {!Promise}
   */
  async runPumpkinParseTestCase(testCase) {
    const macro = await this.getPumpkinParseStrategy().parse(testCase.text);
    this.runParseTestCaseAssertions(testCase, macro);
  }

  /**
   * @param {!ParseTestCase} testCase
   * @param {?Macro} macro
   */
  runParseTestCaseAssertions(testCase, macro) {
    const expectedName = testCase.expectedName;
    const expectedRepeat = testCase.expectedRepeat;
    const expectedSmart = testCase.expectedSmart;
    if (!macro) {
      assertEquals(undefined, expectedName);
      assertEquals(undefined, expectedRepeat);
      assertEquals(undefined, expectedSmart);
      return;
    }

    if (expectedName) {
      assertEquals(expectedName, macro.getNameAsString());
    }
    if (expectedRepeat) {
      assertEquals(expectedRepeat, macro.repeat_);
    }
    if (expectedSmart) {
      assertEquals(expectedSmart, macro.isSmart());
    }
  }
};
