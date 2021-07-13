// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/mock_accessibility_private.js']);
GEN_INCLUDE(['../../common/testing/mock_input_ime.js']);
GEN_INCLUDE(['../../common/testing/mock_input_method_private.js']);
GEN_INCLUDE(['../../common/testing/mock_language_settings_private.js']);

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

    this.dictationEngineId =
        '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

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
   * Generates a function that runs a callback after the Dictation module has
   * loaded.
   * @param {function<>} callback
   * @returns {function<>}
   */
  runAfterDictationLoad(callback) {
    return this.newCallback(async () => {
      await this.waitForDictationModule();
      callback();
    });
  }

  /**
   * Checks that Dictation is the active IME.
   */
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
};

TEST_F('DictationE2ETest', 'SanityCheck', function() {
  this.runAfterDictationLoad(() => {
    assertFalse(this.mockAccessibilityPrivate.getDictationActive());
  })();
});

TEST_F('DictationE2ETest', 'LoadsIMEWhenEnabled', function() {
  this.runAfterDictationLoad(() => {
    this.checkDictationImeInactive();

    this.mockAccessibilityPrivate.callOnToggleDictation(true);
    assertTrue(this.mockAccessibilityPrivate.getDictationActive());
    this.checkDictationImeActive();

    // Turn off Dictation and make sure it removes as IME
    this.mockAccessibilityPrivate.callOnToggleDictation(false);
    assertFalse(this.mockAccessibilityPrivate.getDictationActive());
    this.checkDictationImeInactive();
  })();
});

TEST_F('DictationE2ETest', 'TogglesDictationOffWhenIMEBlur', function() {
  this.runAfterDictationLoad(() => {
    this.checkDictationImeInactive();

    this.mockAccessibilityPrivate.callOnToggleDictation(true);
    assertTrue(this.mockAccessibilityPrivate.getDictationActive());
    this.checkDictationImeActive();

    // Focus an input context.
    this.mockInputIme.callOnFocus(1);
    // Blur the input context. Dictation should get toggled off.
    this.mockInputIme.callOnBlur(1);

    assertFalse(this.mockAccessibilityPrivate.getDictationActive());

    // Now that we've confirmed that Dictation JS tried to toggle Dictation,
    // via AccessibilityPrivate, we can call the onToggleDictation
    // callback as AccessibilityManager would do, to allow Dictation JS to clean
    // up state.
    this.mockAccessibilityPrivate.callOnToggleDictation(false);

    this.checkDictationImeInactive();
  })();
});

TEST_F('DictationE2ETest', 'ResetsPreviousIMEAfterDeactivate', function() {
  this.runAfterDictationLoad(() => {
    // Set something as the active IME.
    this.mockInputMethodPrivate.setCurrentInputMethod('keyboard_cat');
    this.mockLanguageSettingsPrivate.addInputMethod('keyboard_cat');

    // Activate Dictation.
    this.mockAccessibilityPrivate.callOnToggleDictation(true);
    assertTrue(this.mockAccessibilityPrivate.getDictationActive());
    this.checkDictationImeActive();

    // Deactivate Dictation.
    this.mockAccessibilityPrivate.callOnToggleDictation(false);
    this.checkDictationImeInactive('keyboard_cat');
  })();
});
