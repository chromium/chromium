// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-translate-page' is the settings page
 * translate settings.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './add_languages_dialog.js';
import './languages.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

import type {LanguageSettingsMetricsProxy} from './languages_settings_metrics_proxy.js';
import {LanguageSettingsActionType, LanguageSettingsMetricsProxyImpl} from './languages_settings_metrics_proxy.js';
import type {LanguageHelper, LanguagesModel} from './languages_types.js';
import {getTemplate} from './translate_page.html.js';

const SettingsTranslatePageElementBase = PrefsMixin(I18nMixin(PolymerElement));

export class SettingsTranslatePageElement extends
    SettingsTranslatePageElementBase {
  static get is() {
    return 'settings-translate-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Read-only reference to the languages model provided by the
       * 'settings-languages' instance.
       */
      languages: {
        type: Object,
        notify: true,
      },

      languageHelper: Object,

      showAddAlwaysTranslateDialog_: Boolean,
      showAddNeverTranslateDialog_: Boolean,
      addLanguagesDialogLanguages_: Array,
    };
  }

  languages?: LanguagesModel;
  languageHelper: LanguageHelper;
  private showAddAlwaysTranslateDialog_: boolean;
  private showAddNeverTranslateDialog_: boolean;
  private addLanguagesDialogLanguages_:
      chrome.languageSettingsPrivate.Language[]|null;
  private languageSettingsMetricsProxy_: LanguageSettingsMetricsProxy =
      LanguageSettingsMetricsProxyImpl.getInstance();

  private onTargetLanguageChange_() {
    this.languageHelper.setTranslateTargetLanguage(
        this.shadowRoot!.querySelector<HTMLSelectElement>(
                            '#targetLanguage')!.value);
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        LanguageSettingsActionType.CHANGE_TRANSLATE_TARGET);
  }

  /**
   * Helper function to get the text to display in the target language drop down
   * list. Returns the display name in the current UI language and the native
   * name of the language.
   */
  private getTargetLanguageDisplayOption_(
      item: chrome.languageSettingsPrivate.Language): string {
    return this.languageHelper.getFullName(item);
  }

  /**
   * Checks if a Chrome language code is equal to the translate language code.
   * Used in the translate language selector. If the item matches the translate
   * target language, it will set that item as selected.
   */
  private translateLanguageEqual_(
      chromeItemCode: string, translateTarget: string): boolean {
    return chromeItemCode ===
        this.languageHelper.convertLanguageCodeForChrome(translateTarget);
  }

  /**
   * A function used for sorting languages alphabetically by display name.
   */
  private alphabeticalSort_(
      first: chrome.languageSettingsPrivate.Language,
      second: chrome.languageSettingsPrivate.Language) {
    return first.displayName.localeCompare(second.displayName);
  }

  /**
   * A filter function to return true if language is not undefined and has a
   * displayName.
   */
  private hasDisplayName_(language: chrome.languageSettingsPrivate.Language|
                          undefined): boolean {
    return !!language && !!language!.displayName;
  }

  /**
   * Stamps and opens the Add Languages dialog, registering a listener to
   * disable the dialog's dom-if again on close.
   */
  private onAddAlwaysTranslateLanguagesClick_(e: Event) {
    e.preventDefault();
    const translatableLanguages = this.getTranslatableLanguages_();
    this.addLanguagesDialogLanguages_ = translatableLanguages.filter(
        language => !this.languages!.alwaysTranslate.includes(language));
    this.showAddAlwaysTranslateDialog_ = true;
  }

  private onAlwaysTranslateDialogClose_() {
    this.showAddAlwaysTranslateDialog_ = false;
    this.addLanguagesDialogLanguages_ = null;
    const toFocus =
        this.shadowRoot!.querySelector<HTMLElement>('#addAlwaysTranslate');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  /**
   * Helper function fired by the add dialog's on-languages-added event. Adds
   * selected languages to the always-translate languages list.
   */
  private onAlwaysTranslateLanguagesAdded_(e: CustomEvent<string[]>) {
    const languagesToAdd = e.detail;
    languagesToAdd.forEach(languageCode => {
      this.languageHelper.setLanguageAlwaysTranslateState(languageCode, true);
      this.languageSettingsMetricsProxy_.recordSettingsMetric(
          LanguageSettingsActionType.ADD_TO_ALWAYS_TRANSLATE);
    });
  }

  /**
   * Removes a language from the always translate languages list.
   */
  private onRemoveAlwaysTranslateLanguageClick_(
      e: DomRepeatEvent<chrome.languageSettingsPrivate.Language>) {
    const languageCode = e.model.item.code;
    this.languageHelper.setLanguageAlwaysTranslateState(languageCode, false);
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        LanguageSettingsActionType.REMOVE_FROM_ALWAYS_TRANSLATE);
  }

  /**
   * Stamps and opens the Add Languages dialog, registering a listener to
   * disable the dialog's dom-if again on close.
   */
  private onAddNeverTranslateLanguagesClick_(e: Event) {
    e.preventDefault();
    const translatableLanguages = this.getTranslatableLanguages_();
    this.addLanguagesDialogLanguages_ = translatableLanguages.filter(
        language => !this.languages!.neverTranslate.includes(language));
    this.showAddNeverTranslateDialog_ = true;
  }

  private onNeverTranslateDialogClose_() {
    this.showAddNeverTranslateDialog_ = false;
    this.addLanguagesDialogLanguages_ = null;
    const toFocus =
        this.shadowRoot!.querySelector<HTMLElement>('#addNeverTranslate');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  private onNeverTranslateLanguagesAdded_(e: CustomEvent<string[]>) {
    const languagesToAdd = e.detail;
    languagesToAdd.forEach(languageCode => {
      this.languageHelper.disableTranslateLanguage(languageCode);
      this.languageSettingsMetricsProxy_.recordSettingsMetric(
          LanguageSettingsActionType.ADD_TO_NEVER_TRANSLATE);
    });
  }

  /**
   * Removes a language from the never translate languages list.
   */
  private onRemoveNeverTranslateLanguageClick_(
      e: DomRepeatEvent<chrome.languageSettingsPrivate.Language>) {
    const languageCode = e.model.item.code;
    this.languageHelper.enableTranslateLanguage(languageCode);
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        LanguageSettingsActionType.REMOVE_FROM_NEVER_TRANSLATE);
  }

  private onTranslateToggleChange_(e: Event) {
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        (e.target as SettingsToggleButtonElement).checked ?
            LanguageSettingsActionType.ENABLE_TRANSLATE_GLOBALLY :
            LanguageSettingsActionType.DISABLE_TRANSLATE_GLOBALLY);
  }

  /**
   * @return Whether the list has any items.
   */
  private hasSome_(list: any[]): boolean {
    return !!list.length;
  }

  /**
   * @return Whether the list is has the given length.
   */
  private hasLength_(list: any[], length: number): boolean {
    return list.length === length;
  }

  /**
   * Gets the list of languages that chrome can translate
   */
  private getTranslatableLanguages_():
      chrome.languageSettingsPrivate.Language[] {
    return this.languages!.supported.filter(language => {
      return this.isTranslateSupported_(language);
    });
  }

  /**
   * Filters only for translate supported languages
   */
  private isTranslateSupported_(
      language: chrome.languageSettingsPrivate.Language): boolean {
    return this.languageHelper.isTranslateBaseLanguage(language);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-translate-page': SettingsTranslatePageElement;
  }
}

customElements.define(
    SettingsTranslatePageElement.is, SettingsTranslatePageElement);
