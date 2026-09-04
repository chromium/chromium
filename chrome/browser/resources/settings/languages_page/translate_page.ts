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
import 'chrome://resources/cr_elements/icons.html.js';
import './add_languages_dialog.js';
import './languages.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_page/settings_section.js';

import {getCss as getCrSharedStyleCss} from '//resources/cr_elements/cr_shared_style_lit.css.js';
import {getCss as getMdSelectLitCss} from '//resources/cr_elements/md_select_lit.css.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {getLanguageHelperInstance} from './languages.js';
import type {LanguageSettingsMetricsProxy} from './languages_settings_metrics_proxy.js';
import {LanguageSettingsActionType, LanguageSettingsMetricsProxyImpl} from './languages_settings_metrics_proxy.js';
import type {LanguageHelper, LanguagesModel} from './languages_types.js';
import {convertLanguageCodeForChrome, getFullName, isTranslateBaseLanguage} from './languages_util.js';
import {getHtml} from './translate_page.html.js';

const SettingsTranslatePageElementBase =
    PrefServiceObserverMixinLit(I18nMixinLit(CrLitElement));

export class SettingsTranslatePageElement extends
    SettingsTranslatePageElementBase {
  static get is() {
    return 'settings-translate-page';
  }

  static override get styles() {
    return [
      getCrSharedStyleCss(),
      getSettingsSharedCss(),
      getMdSelectLitCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      supportedLanguages_: {type: Array},
      translateTarget_: {type: String},
      alwaysTranslateLanguages_: {type: Array},
      neverTranslateLanguages_: {type: Array},

      showAddAlwaysTranslateDialog_: {type: Boolean},
      showAddNeverTranslateDialog_: {type: Boolean},
      addLanguagesDialogLanguages_: {type: Array},
      translateEnabledPref_: {type: Object},
    };
  }

  protected accessor supportedLanguages_:
      chrome.languageSettingsPrivate.Language[] = [];
  protected accessor translateTarget_: string = '';
  protected accessor alwaysTranslateLanguages_:
      chrome.languageSettingsPrivate.Language[] = [];
  protected accessor neverTranslateLanguages_:
      chrome.languageSettingsPrivate.Language[] = [];
  protected accessor translateEnabledPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  protected accessor showAddAlwaysTranslateDialog_: boolean = false;
  protected accessor showAddNeverTranslateDialog_: boolean = false;
  protected accessor addLanguagesDialogLanguages_:
      chrome.languageSettingsPrivate.Language[] = [];
  private languageHelper_: LanguageHelper;
  private boundOnLanguagesChanged_: ((e: Event) => void)|null = null;
  private languageSettingsMetricsProxy_: LanguageSettingsMetricsProxy =
      LanguageSettingsMetricsProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPrefs({
      'translate.enabled': 'translateEnabledPref_',
    });

    this.languageHelper_ = getLanguageHelperInstance();
    this.boundOnLanguagesChanged_ = (e: Event) => {
      this.onLanguagesChanged_((e as CustomEvent<LanguagesModel>).detail);
    };
    this.languageHelper_.addEventListener(
        'languages-changed', this.boundOnLanguagesChanged_);
    if (this.languageHelper_.languages) {
      this.onLanguagesChanged_(this.languageHelper_.languages);
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.boundOnLanguagesChanged_) {
      this.languageHelper_.removeEventListener(
          'languages-changed', this.boundOnLanguagesChanged_);
      this.boundOnLanguagesChanged_ = null;
    }
  }

  private onLanguagesChanged_(languages: LanguagesModel) {
    this.supportedLanguages_ = languages.supported;
    this.translateTarget_ = languages.translateTarget;
    this.alwaysTranslateLanguages_ = languages.alwaysTranslate;
    this.neverTranslateLanguages_ = languages.neverTranslate;
  }

  protected onTargetLanguageChange_() {
    this.languageHelper_.setTranslateTargetLanguage(
        this.shadowRoot.querySelector<HTMLSelectElement>(
                           '#targetLanguage')!.value);
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        LanguageSettingsActionType.CHANGE_TRANSLATE_TARGET);
  }

  /**
   * Helper function to get the text to display in the target language drop down
   * list. Returns the display name in the current UI language and the native
   * name of the language.
   */
  protected getTargetLanguageDisplayOption_(
      item: chrome.languageSettingsPrivate.Language): string {
    return getFullName(item);
  }

  /**
   * Checks if a Chrome language code is equal to the translate language code.
   * Used in the translate language selector. If the item matches the translate
   * target language, it will set that item as selected.
   */
  protected translateLanguageEqual_(
      chromeItemCode: string, translateTarget?: string): boolean {
    return !!translateTarget &&
        chromeItemCode === convertLanguageCodeForChrome(translateTarget);
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
  protected hasDisplayName_(
      language: chrome.languageSettingsPrivate.Language|undefined): boolean {
    return !!language && !!language.displayName;
  }

  /**
   * Stamps and opens the Add Languages dialog, registering a listener to
   * disable the dialog's dom-if again on close.
   */
  protected onAddAlwaysTranslateLanguagesClick_(e: Event) {
    e.preventDefault();
    const translatableLanguages = this.getTranslatableLanguages_();
    this.addLanguagesDialogLanguages_ = translatableLanguages.filter(
        language => !this.alwaysTranslateLanguages_.includes(language));
    this.showAddAlwaysTranslateDialog_ = true;
  }

  protected onAlwaysTranslateDialogClose_() {
    this.showAddAlwaysTranslateDialog_ = false;
    this.addLanguagesDialogLanguages_ = [];
    const toFocus =
        this.shadowRoot.querySelector<HTMLElement>('#addAlwaysTranslate');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  /**
   * Helper function fired by the add dialog's on-languages-added event. Adds
   * selected languages to the always-translate languages list.
   */
  protected onAlwaysTranslateLanguagesAdded_(e: CustomEvent<string[]>) {
    const languagesToAdd = e.detail;
    languagesToAdd.forEach(languageCode => {
      this.languageHelper_.setLanguageAlwaysTranslateState(languageCode, true);
      this.languageSettingsMetricsProxy_.recordSettingsMetric(
          LanguageSettingsActionType.ADD_TO_ALWAYS_TRANSLATE);
    });
  }

  /**
   * Removes a language from the always translate languages list.
   */
  protected onRemoveAlwaysTranslateLanguageClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const languageCode = target.dataset['code']!;
    this.languageHelper_.setLanguageAlwaysTranslateState(languageCode, false);
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        LanguageSettingsActionType.REMOVE_FROM_ALWAYS_TRANSLATE);
  }

  /**
   * Stamps and opens the Add Languages dialog, registering a listener to
   * disable the dialog's dom-if again on close.
   */
  protected onAddNeverTranslateLanguagesClick_(e: Event) {
    e.preventDefault();
    const translatableLanguages = this.getTranslatableLanguages_();
    this.addLanguagesDialogLanguages_ = translatableLanguages.filter(
        language => !this.neverTranslateLanguages_.includes(language));
    this.showAddNeverTranslateDialog_ = true;
  }

  protected onNeverTranslateDialogClose_() {
    this.showAddNeverTranslateDialog_ = false;
    this.addLanguagesDialogLanguages_ = [];
    const toFocus =
        this.shadowRoot.querySelector<HTMLElement>('#addNeverTranslate');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  /**
   * Removes a language from the never translate languages list.
   */
  protected onNeverTranslateLanguagesAdded_(e: CustomEvent<string[]>) {
    const languagesToAdd = e.detail;
    languagesToAdd.forEach(languageCode => {
      this.languageHelper_.disableTranslateLanguage(languageCode);
      this.languageSettingsMetricsProxy_.recordSettingsMetric(
          LanguageSettingsActionType.ADD_TO_NEVER_TRANSLATE);
    });
  }

  /**
   * Removes a language from the never translate languages list.
   */
  protected onRemoveNeverTranslateLanguageClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const languageCode = target.dataset['code']!;
    this.languageHelper_.enableTranslateLanguage(languageCode);
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        LanguageSettingsActionType.REMOVE_FROM_NEVER_TRANSLATE);
  }

  protected onOfferTranslateOtherLanguagesSettingsBooleanControlChange_(
      e: Event) {
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        (e.target as SettingsToggleButtonElement).checked ?
            LanguageSettingsActionType.ENABLE_TRANSLATE_GLOBALLY :
            LanguageSettingsActionType.DISABLE_TRANSLATE_GLOBALLY);
  }

  /**
   * @return Whether the list has any items.
   */
  protected hasSome_(list?: unknown[]): boolean {
    return !!list?.length;
  }

  /**
   * @return Whether the delete button for never translate languages should be
   *     disabled.
   */
  protected shouldDisableDeleteNeverTranslateLanguage_(): boolean {
    return this.neverTranslateLanguages_.length === 1;
  }

  protected getSupportedLanguages_():
      chrome.languageSettingsPrivate.Language[] {
    return this.supportedLanguages_.filter(
        language => this.isTranslateSupported_(language));
  }

  protected getAlwaysTranslateLanguages_():
      chrome.languageSettingsPrivate.Language[] {
    return this.alwaysTranslateLanguages_
        .filter(lang => this.hasDisplayName_(lang))
        .slice()
        .sort(this.alphabeticalSort_);
  }

  protected getNeverTranslateLanguages_():
      chrome.languageSettingsPrivate.Language[] {
    return this.neverTranslateLanguages_
        .filter(lang => this.hasDisplayName_(lang))
        .slice()
        .sort(this.alphabeticalSort_);
  }

  /**
   * Gets the list of languages that chrome can translate
   */
  private getTranslatableLanguages_():
      chrome.languageSettingsPrivate.Language[] {
    return this.supportedLanguages_.filter(language => {
      return this.isTranslateSupported_(language);
    });
  }

  /**
   * Filters only for translate supported languages
   */
  private isTranslateSupported_(
      language: chrome.languageSettingsPrivate.Language): boolean {
    return isTranslateBaseLanguage(language);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-translate-page': SettingsTranslatePageElement;
  }
}

customElements.define(
    SettingsTranslatePageElement.is, SettingsTranslatePageElement);
