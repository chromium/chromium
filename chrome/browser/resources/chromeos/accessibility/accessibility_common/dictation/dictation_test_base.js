// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/mock_accessibility_private.js']);
GEN_INCLUDE(['../../common/testing/mock_input_ime.js']);
GEN_INCLUDE(['../../common/testing/mock_input_method_private.js']);
GEN_INCLUDE(['../../common/testing/mock_language_settings_private.js']);
GEN_INCLUDE(['../../common/testing/mock_speech_recognition_private.js']);

/**
 * Base class for tests for Dictation feature using accessibility common
 * extension browser tests.
 */
DictationE2ETestBase = class extends E2ETestBase {
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

    this.mockSpeechRecognitionPrivate = new MockSpeechRecognitionPrivate();
    chrome.speechRecognitionPrivate = this.mockSpeechRecognitionPrivate;

    this.dictationEngineId =
        '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

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
    assertTrue(this.mockSpeechRecognitionPrivate.isStarted());
  }

  /**
   * Turns Dictation off from AccessibilityPrivate and checks IME and Speech
   * Recognition state. Note that Dictation can also be toggled off by blurring
   * the current input context, SR errors, or timeouts.
   */
  toggleDictationOffFromA11yPrivate() {
    this.mockAccessibilityPrivate.callOnToggleDictation(false);
    assertFalse(
        this.mockAccessibilityPrivate.getDictationActive(),
        'Dictation should be inactive after toggling Dictation');
    this.checkDictationImeInactive();
    assertFalse(
        this.mockSpeechRecognitionPrivate.isStarted(),
        'Speech recognition should be off');
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
  }

  mockSetTimeoutMethod() {
    setTimeout = (callback, delay) => {
      this.lastSetTimeoutCallback = callback;
      this.lastSetDelay = delay;
    };
  }

  /**
   * Enables commands feature for testing.
   */
  async setCommandsEnabledForTest(enabled) {
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
  async assertImeCommitParameters(text, contextID) {
    if (!this.mockInputIme.getLastCommittedParameters()) {
      await this.mockInputIme.waitForCommit();
    }
    assertEquals(text, this.mockInputIme.getLastCommittedParameters().text);
    assertEquals(
        contextID, this.mockInputIme.getLastCommittedParameters().contextID);
  }

  /** Sets up Dictation with commands enabled. */
  async waitForDictationWithCommands() {
    await this.waitForDictationModule();
    await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'en-US');
    await this.setCommandsEnabledForTest(true);
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
};