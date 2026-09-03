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
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../controls/controlled_radio_button.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixin} from '/shared/settings/prefs2/pref_service_observer_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getLanguageHelperInstance} from './languages.js';
import type {LanguageSettingsMetricsProxy} from './languages_settings_metrics_proxy.js';
import {LanguageSettingsActionType, LanguageSettingsMetricsProxyImpl} from './languages_settings_metrics_proxy.js';
import type {LanguageHelper, LanguagesModel, LanguageState, SpellCheckLanguageState} from './languages_types.js';
import {getTemplate} from './spell_check_page.html.js';

const SettingsSpellCheckPageElementBase = SettingsViewMixin(
    I18nMixin(PrefServiceObserverMixin(BaseMixin(PolymerElement))));

export class SettingsSpellCheckPageElement extends
    SettingsSpellCheckPageElementBase {
  static get is() {
    return 'settings-spell-check-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Read-only reference to the languages model provided by the
       * 'settings-languages' instance.
       */
      languages: Object,

      // <if expr="not is_macosx">
      spellCheckLanguages_: {
        type: Array,
        value() {
          return [];
        },
      },

      hideSpellCheckLanguages_: {
        type: Boolean,
        computed: 'computeHideSpellCheckLanguages_(spellCheckLanguages_.*)',
      },
      // </if>

      enableSpellcheckingPref_: Object,
      useSpellingServicePref_: Object,
      forcedDictionariesPref_: Object,
      blockedDictionariesPref_: Object,
    };
  }

  // <if expr="not is_macosx">
  static get observers() {
    return [
      'updateSpellcheckLanguages_(languages, languages.enabled.*, ' +
          'languages.spellCheckOnLanguages.*)',
      'updateSpellcheckEnabled_(enableSpellcheckingPref_)',
    ];
  }
  // </if>

  declare languages?: LanguagesModel;
  declare protected enableSpellcheckingPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  declare protected useSpellingServicePref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  declare protected forcedDictionariesPref_:
      chrome.settingsPrivate.PrefObject<string[]>|undefined;
  declare protected blockedDictionariesPref_:
      chrome.settingsPrivate.PrefObject<string[]>|undefined;
  // <if expr="not is_macosx">
  declare private spellCheckLanguages_:
      Array<LanguageState|SpellCheckLanguageState>;
  declare private hideSpellCheckLanguages_: boolean;
  // </if>
  private languageHelper_: LanguageHelper;
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

    this.languageHelper_ = getLanguageHelperInstance();
  }

  private onSpellCheckToggleChange_(e: Event) {
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        (e.target as SettingsToggleButtonElement).checked ?
            LanguageSettingsActionType.ENABLE_SPELL_CHECK_GLOBALLY :
            LanguageSettingsActionType.DISABLE_SPELL_CHECK_GLOBALLY);
  }

  private onSelectedSpellingServiceChange_() {
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
  private errorsGreaterThan_(
      downloadDictionaryFailureCount: number, threshold: number): boolean {
    return downloadDictionaryFailureCount > threshold;
  }
  // </if>

  // <if expr="not is_macosx">
  /**
   * Returns the value to use as the |pref| attribute for the policy indicator
   * of spellcheck languages, based on whether or not the language is enabled.
   * @param isEnabled Whether the language is enabled or not.
   */
  private getIndicatorPrefForManagedSpellcheckLanguage_(isEnabled: boolean):
      chrome.settingsPrivate.PrefObject {
    return isEnabled ? this.forcedDictionariesPref_! :
                       this.blockedDictionariesPref_!;
  }

  /**
   * Returns an array of enabled languages, plus spellcheck languages that are
   * force-enabled by policy.
   */
  private getSpellCheckLanguages_():
      Array<LanguageState|SpellCheckLanguageState> {
    const supportedSpellcheckLanguages:
        Array<LanguageState|SpellCheckLanguageState> =
            this.languages!.enabled.filter(
                (item) => item.language.supportsSpellcheck);
    const supportedSpellcheckLanguagesSet =
        new Set(supportedSpellcheckLanguages.map(x => x.language.code));

    this.languages!.spellCheckOnLanguages.forEach(spellCheckLang => {
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
  private computeHideSpellCheckLanguages_(): boolean {
    if (this.spellCheckLanguages_ && this.spellCheckLanguages_.length === 1) {
      const singleLanguage = this.spellCheckLanguages_[0];
      return !singleLanguage.isManaged &&
          singleLanguage.downloadDictionaryFailureCount === 0;
    }
    return false;
  }

  private updateSpellcheckLanguages_() {
    if (this.languages === undefined) {
      return;
    }

    this.set('spellCheckLanguages_', this.getSpellCheckLanguages_());

    // Notify Polymer of subproperties that might have changed on the items in
    // the spellCheckLanguages_ array, to make sure the UI updates. Polymer
    // would otherwise not notice the changes in the subproperties, as some of
    // them are references to those from |this.languages.enabled|. It would be
    // possible to |this.linkPaths()| objects from |this.languages.enabled| to
    // |this.spellCheckLanguages_|, but that would require complex
    // housekeeping to |this.unlinkPaths()| as |this.languages.enabled|
    // changes.
    for (let i = 0; i < this.spellCheckLanguages_.length; i++) {
      this.notifyPath(`spellCheckLanguages_.${i}.isManaged`);
      this.notifyPath(`spellCheckLanguages_.${i}.spellCheckEnabled`);
      this.notifyPath(
          `spellCheckLanguages_.${i}.downloadDictionaryFailureCount`);
    }

    if (this.spellCheckLanguages_.length === 0) {
      // If there are no supported spell check languages, automatically turn
      // off spell check to indicate no spell check will happen.
      PrefService.getInstance().setPrefValue<boolean>(
          'browser.enable_spellchecking', false);
    }
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
  private onEditDictionaryClick_() {
    Router.getInstance().navigateTo(routes.EDIT_DICTIONARY);
  }

  /**
   * Handler for enabling or disabling spell check for a specific language.
   */
  private onSpellCheckLanguageChange_(
      e: DomRepeatEvent<LanguageState|SpellCheckLanguageState>) {
    const item = e.model.item;
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
  private onRetryDictionaryDownloadClick_(
      e: DomRepeatEvent<LanguageState|SpellCheckLanguageState>) {
    assert(this.errorsGreaterThan_(
        e.model.item.downloadDictionaryFailureCount, 0));
    this.languageHelper_.retryDownloadDictionary(e.model.item.language.code);
  }

  /**
   * Handler for clicking on the name of the language. The action taken must
   * match the control that is available.
   */
  private onSpellCheckNameClick_(
      e: DomRepeatEvent<LanguageState|SpellCheckLanguageState>) {
    assert(!this.isSpellCheckNameClickDisabled_(e.model.item));
    this.onSpellCheckLanguageChange_(e);
  }

  /**
   * Name only supports clicking when language is not managed, supports
   * spellcheck, and the dictionary has been downloaded with no errors.
   */
  private isSpellCheckNameClickDisabled_(item: LanguageState|
                                         SpellCheckLanguageState): boolean {
    return item.isManaged || !item.language.supportsSpellcheck ||
        item.downloadDictionaryFailureCount > 0;
  }
  // </if> expr="not is_macosx"

  private getSpellCheckSubLabel_(): string|undefined {
    // <if expr="not is_macosx">
    if (this.spellCheckLanguages_.length === 0) {
      return this.i18n('spellCheckDisabledReason');
    }
    // </if>
    return undefined;
  }

  /**
   * Toggles the expand button within the element being listened to.
   */
  private toggleExpandButton_(e: Event) {
    // The expand button handles toggling itself.
    if ((e.target as HTMLElement).tagName === 'CR-EXPAND-BUTTON') {
      return;
    }

    if (!(e.currentTarget as HTMLElement).hasAttribute('actionable')) {
      return;
    }

    const expandButton =
        (e.currentTarget as HTMLElement).querySelector('cr-expand-button')!;
    assert(expandButton);
    expandButton.expanded = !expandButton.expanded;
    focusWithoutInk(expandButton);
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
    const control = this.shadowRoot!.querySelector<HTMLElement>(
        '#spellCheckSubpageTrigger');
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
