// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-spell-check-page' is the settings page
 * for spell check settings.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
// <if expr="_google_chrome or not is_macosx">
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
// </if>
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../controls/controlled_radio_button.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_section.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
// <if expr="_google_chrome or not is_macosx">
import type {CrCollapseElement} from 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
// </if>
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

// <if expr="_google_chrome">
import type {ControlledRadioButtonElement} from '../controls/controlled_radio_button.js';
// </if>
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

// <if expr="not is_macosx">
import {getLanguageHelperInstance} from './languages.js';
// </if>
import type {LanguageSettingsMetricsProxy} from './languages_settings_metrics_proxy.js';
import {LanguageSettingsActionType, LanguageSettingsMetricsProxyImpl} from './languages_settings_metrics_proxy.js';
import type {LanguageHelper, LanguagesModel, LanguageState, SpellCheckLanguageState} from './languages_types.js';
import {getCss} from './spell_check_page.css.js';
import {getHtml} from './spell_check_page.html.js';

export interface SettingsSpellCheckPageElement {
  $: {
    enableSpellcheckingToggle: SettingsToggleButtonElement,
    // <if expr="_google_chrome or not is_macosx">
    spellCheckCollapse: CrCollapseElement,
    // </if>
    // <if expr="not is_macosx">
    spellCheckLanguagesList: HTMLElement,
    // </if>
    // <if expr="_google_chrome">
    spellingServiceEnable: ControlledRadioButtonElement,
    // </if>
  };
}

export type SpellCheckPageElement = SettingsSpellCheckPageElement;

const SettingsSpellCheckPageElementBase = SettingsViewMixinLit(
    I18nMixinLit(PrefServiceObserverMixinLit(CrLitElement)));

export class SettingsSpellCheckPageElement extends
    SettingsSpellCheckPageElementBase {
  static get is() {
    return 'settings-spell-check-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // <if expr="not is_macosx">
      spellCheckLanguages_: {type: Array},
      // </if>

      enableSpellcheckingPref_: {type: Object},
      useSpellingServicePref_: {type: Object},
      forcedDictionariesPref_: {type: Object},
      blockedDictionariesPref_: {type: Object},
    };
  }

  protected accessor enableSpellcheckingPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  protected accessor useSpellingServicePref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  protected accessor forcedDictionariesPref_:
      chrome.settingsPrivate.PrefObject<string[]>|undefined;
  protected accessor blockedDictionariesPref_:
      chrome.settingsPrivate.PrefObject<string[]>|undefined;
  // <if expr="not is_macosx">
  protected accessor spellCheckLanguages_:
      Array<LanguageState|SpellCheckLanguageState> = [];
  private languageHelper_: LanguageHelper;
  private boundOnLanguagesChanged_: ((e: Event) => void)|null = null;
  // </if>
  private languageSettingsMetricsProxy_: LanguageSettingsMetricsProxy =
      LanguageSettingsMetricsProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPrefs({
      'browser.enable_spellchecking': 'enableSpellcheckingPref_',
      'spellcheck.use_spelling_service': 'useSpellingServicePref_',
      'spellcheck.forced_dictionaries': 'forcedDictionariesPref_',
      'spellcheck.blocked_dictionaries': 'blockedDictionariesPref_',
    });

    // <if expr="not is_macosx">
    this.languageHelper_ = getLanguageHelperInstance();
    this.boundOnLanguagesChanged_ = (e: Event) => {
      this.updateSpellcheckLanguages_(
          (e as CustomEvent<LanguagesModel>).detail);
    };
    this.languageHelper_.addEventListener(
        'languages-changed', this.boundOnLanguagesChanged_);
    if (this.languageHelper_.languages) {
      this.updateSpellcheckLanguages_(this.languageHelper_.languages);
    }
    // </if>
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    // <if expr="not is_macosx">
    if (this.boundOnLanguagesChanged_) {
      this.languageHelper_.removeEventListener(
          'languages-changed', this.boundOnLanguagesChanged_);
      this.boundOnLanguagesChanged_ = null;
    }
    // </if>
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    // <if expr="not is_macosx">
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('enableSpellcheckingPref_')) {
      this.updateSpellcheckEnabled_();
    }
    // </if>
  }

  protected onEnableSpellcheckingToggleSettingsBooleanControlChange_(e: Event) {
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        (e.target as SettingsToggleButtonElement).checked ?
            LanguageSettingsActionType.ENABLE_SPELL_CHECK_GLOBALLY :
            LanguageSettingsActionType.DISABLE_SPELL_CHECK_GLOBALLY);
  }

  protected onSelectedSpellingServiceChange_() {
    assert(this.useSpellingServicePref_);
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        this.useSpellingServicePref_.value ?
            LanguageSettingsActionType.SELECT_ENHANCED_SPELL_CHECK :
            LanguageSettingsActionType.SELECT_BASIC_SPELL_CHECK);
  }

  // <if expr="not is_macosx">
  /**
   * Checks if there are any errors downloading the spell check dictionary.
   * This is used for showing/hiding error messages, spell check toggle and
   * retry. button.
   */
  protected errorsGreaterThan_(
      downloadDictionaryFailureCount: number, threshold: number): boolean {
    return downloadDictionaryFailureCount > threshold;
  }

  /**
   * Returns the value to use as the |pref| attribute for the policy indicator
   * of spellcheck languages, based on whether or not the language is enabled.
   * @param isEnabled Whether the language is enabled or not.
   */
  protected getIndicatorPrefForManagedSpellcheckLanguage_(isEnabled: boolean):
      chrome.settingsPrivate.PrefObject {
    return isEnabled ? this.forcedDictionariesPref_! :
                       this.blockedDictionariesPref_!;
  }

  /**
   * Returns an array of enabled languages, plus spellcheck languages that are
   * force-enabled by policy.
   */
  private getSpellCheckLanguages_(languages: LanguagesModel):
      Array<LanguageState|SpellCheckLanguageState> {
    const supportedSpellcheckLanguages:
        Array<LanguageState|SpellCheckLanguageState> = languages.enabled.filter(
            (item) => item.language.supportsSpellcheck);
    const supportedSpellcheckLanguagesSet =
        new Set(supportedSpellcheckLanguages.map(x => x.language.code));

    languages.spellCheckOnLanguages.forEach(spellCheckLang => {
      if (!supportedSpellcheckLanguagesSet.has(spellCheckLang.language.code)) {
        supportedSpellcheckLanguages.push(spellCheckLang);
      }
    });
    return supportedSpellcheckLanguages;
  }

  /**
   * Hide list of spell check languages if there is only 1 language and we don't
   * need to display any errors or management indicators for that language.
   */
  protected shouldHideSpellCheckLanguages_(): boolean {
    if (this.spellCheckLanguages_.length === 1) {
      const singleLanguage = this.spellCheckLanguages_[0];
      return !singleLanguage.isManaged &&
          singleLanguage.downloadDictionaryFailureCount === 0;
    }
    return false;
  }

  private updateSpellcheckLanguages_(languages?: LanguagesModel) {
    if (!languages) {
      this.spellCheckLanguages_ = [];
      return;
    }

    this.spellCheckLanguages_ = this.getSpellCheckLanguages_(languages);

    if (this.spellCheckLanguages_.length === 0) {
      // If there are no supported spell check languages, automatically turn
      // off spell check to indicate no spell check will happen.
      PrefService.getInstance().setPrefValue<boolean>(
          'browser.enable_spellchecking', false);
    }

    this.updateSpellcheckEnabled_();
  }

  private updateSpellcheckEnabled_() {
    if (this.enableSpellcheckingPref_ === undefined) {
      return;
    }

    // If there is only 1 language, we hide the list of languages so users
    // are unable to toggle on/off spell check specifically for the 1
    // language. Therefore, we need to treat the toggle for
    // `browser.enable_spellchecking` as the toggle for the 1 language as
    // well.
    if (this.spellCheckLanguages_.length === 1) {
      // Need to call getLanguageHelperInstance() instead of
      // this.languageHelper_ here, because Polymer observers fire before
      // connectedCallback sometimes.
      getLanguageHelperInstance().toggleSpellCheck(
          this.spellCheckLanguages_[0].language.code,
          this.enableSpellcheckingPref_.value);
    }
  }

  /**
   * Opens the Custom Dictionary page.
   */
  protected onEditDictionaryClick_() {
    Router.getInstance().navigateTo(routes.EDIT_DICTIONARY);
  }

  /**
   * Handler for enabling or disabling spell check for a specific language.
   */
  protected onSpellCheckLanguageChange_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    const item = this.spellCheckLanguages_[index];
    assert(item);
    this.changeSpellCheckLanguage_(item);
  }

  private changeSpellCheckLanguage_(
      item: LanguageState|SpellCheckLanguageState) {
    if (!item.language.supportsSpellcheck) {
      return;
    }

    const enable = !item.spellCheckEnabled;
    this.languageHelper_.toggleSpellCheck(item.language.code, enable);

    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        enable ? LanguageSettingsActionType.ENABLE_SPELL_CHECK_FOR_LANGUAGE :
                 LanguageSettingsActionType.DISABLE_SPELL_CHECK_FOR_LANGUAGE);
  }

  /**
   * Handler to initiate another attempt at downloading the spell check
   * dictionary for a specified language.
   */
  protected onRetryDictionaryDownloadClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    const item = this.spellCheckLanguages_[index];
    assert(item);
    assert(this.errorsGreaterThan_(item.downloadDictionaryFailureCount, 0));
    this.languageHelper_.retryDownloadDictionary(item.language.code);
  }

  /**
   * Handler for clicking on the name of the language. The action taken must
   * match the control that is available.
   */
  protected onSpellCheckNameClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    const item = this.spellCheckLanguages_[index];
    assert(item);
    if (this.isSpellCheckNameClickDisabled_(item)) {
      return;
    }
    this.changeSpellCheckLanguage_(item);
  }

  /**
   * Name only supports clicking when language is not managed, supports
   * spellcheck, and the dictionary has been downloaded with no errors.
   */
  protected isSpellCheckNameClickDisabled_(
      item: LanguageState|SpellCheckLanguageState): boolean {
    return item.isManaged || !item.language.supportsSpellcheck ||
        item.downloadDictionaryFailureCount > 0;
  }
  // </if> expr="not is_macosx"

  protected isSpellCheckToggleDisabled_(): boolean {
    // <if expr="not is_macosx">
    return !this.spellCheckLanguages_.length;
    // </if>
    // <if expr="is_macosx">
    return false;
    // </if>
  }

  protected getSpellCheckSubLabel_(): string {
    // <if expr="not is_macosx">
    if (this.spellCheckLanguages_.length === 0) {
      return this.i18n('spellCheckDisabledReason');
    }
    // </if>
    return '';
  }

  // <if expr="not is_macosx">
  // SettingsViewMixin implementation.
  override getFocusConfig() {
    const map = new Map();
    if (routes.EDIT_DICTIONARY) {
      map.set(routes.EDIT_DICTIONARY.path, '#spellCheckSubpageTrigger');
    }
    return map;
  }
  // </if>

  // SettingsViewMixin implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    assert(childViewId === 'editDictionary');
    const control =
        this.shadowRoot.querySelector<HTMLElement>('#spellCheckSubpageTrigger');
    assert(
        control,
        `Failed to find associated control for child '${childViewId}'`);
    return control;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-spell-check-page': SettingsSpellCheckPageElement;
  }
}

customElements.define(
    SettingsSpellCheckPageElement.is, SettingsSpellCheckPageElement);
