// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Handles metrics for ChromeOS's languages OS settings.
 * TODO(crbug/1109431): Remove these metrics when languages settings migration
 * is completed and data analysed.
 */

/**
 * Keeps in sync with SettingsLanguagesPageInteraction
 * in tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const LanguagesPageInteraction = {
  SWITCH_SYSTEM_LANGUAGE: 0,
  RESTART: 1,
  SWITCH_INPUT_METHOD: 2,
  RESTART_LATER: 3,
  OPEN_CUSTOM_SPELL_CHECK: 4,
  OPEN_MANAGE_GOOGLE_ACCOUNT_LANGUAGE: 5,
  OPEN_WEB_LANGUAGES_LEARN_MORE: 6,
  OPEN_LANGUAGE_PACKS_LEARN_MORE: 7,
};

/**
 * Keeps in sync with SettingsInputsShortcutReminderState
 * in tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const InputsShortcutReminderState = {
  NONE: 0,
  LAST_USED_IME: 1,
  NEXT_IME: 2,
  LAST_USED_IME_AND_NEXT_IME: 3,
};

/** @interface */
export class LanguagesMetricsProxy {
  /**
   * Records the interaction to enumerated histogram.
   * @param {!LanguagesPageInteraction} interaction
   */
  recordInteraction(interaction) {}

  /** Records when users select "Add input method". */
  recordAddInputMethod() {}

  /** Records when users select "Add languages". */
  recordAddLanguages() {}

  /** Records when users select "Manage input methods". */
  recordManageInputMethods() {}

  /**
   * Records when users toggle "Show Input Options On Shelf" option.
   * @param {boolean} value
   */
  recordToggleShowInputOptionsOnShelf(value) {}

  /**
   * Records when users toggle "Spell check" option.
   * @param {boolean} value
   */
  recordToggleSpellCheck(value) {}

  /**
   * Records when users toggle "Offer to translate languages you don't read"
   * option.
   * @param {boolean} value
   */
  recordToggleTranslate(value) {}

  /**
   * Records when users check/uncheck "Offer to translate pages in this
   * language" checkbox.
   * @param {boolean} value
   */
  recordTranslateCheckboxChanged(value) {}

  /**
   * Records when users dismiss the shortcut reminder.
   * @param {InputsShortcutReminderState} value
   */
  recordShortcutReminderDismissed(value) {}
}

/** @type {?LanguagesMetricsProxy} */
let instance = null;

/** @implements {LanguagesMetricsProxy} */
export class LanguagesMetricsProxyImpl {
  /** @return {!LanguagesMetricsProxy} */
  static getInstance() {
    return instance || (instance = new LanguagesMetricsProxyImpl());
  }

  /** @param {!LanguagesMetricsProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  recordInteraction(interaction) {
    chrome.metricsPrivate.recordEnumerationValue(
        'ChromeOS.Settings.Languages.Interaction', interaction,
        Object.keys(LanguagesPageInteraction).length);
  }

  /** @override */
  recordAddInputMethod() {
    chrome.metricsPrivate.recordUserAction(
        'ChromeOS.Settings.Languages.AddInputMethod');
  }

  /** @override */
  recordAddLanguages() {
    chrome.metricsPrivate.recordUserAction(
        'ChromeOS.Settings.Languages.AddLanguages');
  }

  /** @override */
  recordManageInputMethods() {
    chrome.metricsPrivate.recordUserAction(
        'ChromeOS.Settings.Languages.ManageInputMethods');
  }

  /** @override */
  recordToggleShowInputOptionsOnShelf(value) {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.Settings.Languages.Toggle.ShowInputOptionsOnShelf', value);
  }

  /** @override */
  recordToggleSpellCheck(value) {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.Settings.Languages.Toggle.SpellCheck', value);
  }

  /** @override */
  recordToggleTranslate(value) {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.Settings.Languages.Toggle.Translate', value);
  }

  /** @override */
  recordTranslateCheckboxChanged(value) {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.Settings.Languages.OfferToTranslateCheckbox', value);
  }

  /** @override */
  recordShortcutReminderDismissed(value) {
    chrome.metricsPrivate.recordEnumerationValue(
        'ChromeOS.Settings.Inputs.ShortcutReminderDismissed', value,
        Object.keys(InputsShortcutReminderState).length);
  }
}
