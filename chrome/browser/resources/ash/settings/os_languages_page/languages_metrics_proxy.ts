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
 */
export enum LanguagesPageInteraction {
  SWITCH_SYSTEM_LANGUAGE = 0,
  RESTART = 1,
  SWITCH_INPUT_METHOD = 2,
  RESTART_LATER = 3,
  OPEN_CUSTOM_SPELL_CHECK = 4,
  OPEN_MANAGE_GOOGLE_ACCOUNT_LANGUAGE = 5,
  OPEN_WEB_LANGUAGES_LEARN_MORE = 6,
  OPEN_LANGUAGE_PACKS_LEARN_MORE = 7,
}

/**
 * Keeps in sync with SettingsInputsShortcutReminderState
 * in tools/metrics/histograms/enums.xml.
 */
export enum InputsShortcutReminderState {
  NONE = 0,
  LAST_USED_IME = 1,
  NEXT_IME = 2,
  LAST_USED_IME_AND_NEXT_IME = 3,
}

export interface LanguagesMetricsProxy {
  /**
   * Records the interaction to enumerated histogram.
   */
  recordInteraction(interaction: LanguagesPageInteraction): void;

  /** Records when users select "Add input method". */
  recordAddInputMethod(): void;

  /** Records when users select "Add languages". */
  recordAddLanguages(): void;

  /** Records when users select "Manage input methods". */
  recordManageInputMethods(): void;

  /**
   * Records when users toggle "Show Input Options On Shelf" option.
   */
  recordToggleShowInputOptionsOnShelf(value: boolean): void;

  /**
   * Records when users toggle "Spell check" option.
   */
  recordToggleSpellCheck(value: boolean): void;

  /**
   * Records when users toggle "Offer to translate languages you don't read"
   * option.
   */
  recordToggleTranslate(value: boolean): void;

  /**
   * Records when users check/uncheck "Offer to translate pages in this
   * language" checkbox.
   */
  recordTranslateCheckboxChanged(value: boolean): void;

  /**
   * Records when users dismiss the shortcut reminder.
   */
  recordShortcutReminderDismissed(value: InputsShortcutReminderState): void;
}

let instance: LanguagesMetricsProxy|null = null;

export class LanguagesMetricsProxyImpl implements LanguagesMetricsProxy {
  static getInstance(): LanguagesMetricsProxy {
    return instance || (instance = new LanguagesMetricsProxyImpl());
  }

  static setInstanceForTesting(obj: LanguagesMetricsProxy): void {
    instance = obj;
  }

  recordInteraction(interaction: LanguagesPageInteraction): void {
    chrome.metricsPrivate.recordEnumerationValue(
        'ChromeOS.Settings.Languages.Interaction', interaction,
        Object.keys(LanguagesPageInteraction).length);
  }

  recordAddInputMethod(): void {
    chrome.metricsPrivate.recordUserAction(
        'ChromeOS.Settings.Languages.AddInputMethod');
  }

  recordAddLanguages(): void {
    chrome.metricsPrivate.recordUserAction(
        'ChromeOS.Settings.Languages.AddLanguages');
  }

  recordManageInputMethods(): void {
    chrome.metricsPrivate.recordUserAction(
        'ChromeOS.Settings.Languages.ManageInputMethods');
  }

  recordToggleShowInputOptionsOnShelf(value: boolean): void {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.Settings.Languages.Toggle.ShowInputOptionsOnShelf', value);
  }

  recordToggleSpellCheck(value: boolean): void {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.Settings.Languages.Toggle.SpellCheck', value);
  }

  recordToggleTranslate(value: boolean): void {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.Settings.Languages.Toggle.Translate', value);
  }

  recordTranslateCheckboxChanged(value: boolean): void {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.Settings.Languages.OfferToTranslateCheckbox', value);
  }

  recordShortcutReminderDismissed(value: InputsShortcutReminderState): void {
    chrome.metricsPrivate.recordEnumerationValue(
        'ChromeOS.Settings.Inputs.ShortcutReminderDismissed', value,
        Object.keys(InputsShortcutReminderState).length);
  }
}
