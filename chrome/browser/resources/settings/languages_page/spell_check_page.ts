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
import 'chrome://resources/cr_elements/icons_lit.html.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../controls/controlled_radio_button.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
// <if expr="not is_macosx">
import './edit_dictionary_page.js';

// </if>

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import type {FocusConfig} from '../focus_config.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import type {LanguageSettingsMetricsProxy} from './languages_settings_metrics_proxy.js';
import {LanguageSettingsActionType, LanguageSettingsMetricsProxyImpl} from './languages_settings_metrics_proxy.js';
import type {LanguageHelper, LanguagesModel, LanguageState, SpellCheckLanguageState} from './languages_types.js';
import {getTemplate} from './spell_check_page.html.js';

const SettingsSpellCheckPageElementBase =
    I18nMixin(PrefsMixin(BaseMixin(PolymerElement)));

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

      // <if expr="not is_macosx">
      spellCheckLanguages_: {
        type: Array,
        value() {
          return [];
        },
      },
      // </if>

      hideSpellCheckLanguages_: {
        type: Boolean,
        value: false,
      },

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          // <if expr="not is_macosx">
          if (routes.EDIT_DICTIONARY) {
            map.set(routes.EDIT_DICTIONARY.path, '#spellCheckSubpageTrigger');
          }
          // </if>
          return map;
        },
      },
    };
  }

  // <if expr="not is_macosx">
  static get observers() {
    return [
      'updateSpellcheckLanguages_(languages.enabled.*, ' +
          'languages.spellCheckOnLanguages.*)',
      'updateSpellcheckEnabled_(prefs.browser.enable_spellchecking.*)',
    ];
  }
  // </if>

  languages?: LanguagesModel;
  languageHelper: LanguageHelper;
  private spellCheckLanguages_: Array<LanguageState|SpellCheckLanguageState>;
  private hideSpellCheckLanguages_: boolean;
  private focusConfig_: FocusConfig;
  private languageSettingsMetricsProxy_: LanguageSettingsMetricsProxy =
      LanguageSettingsMetricsProxyImpl.getInstance();

  private onSpellCheckToggleChange_(e: Event) {
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        (e.target as SettingsToggleButtonElement).checked ?
            LanguageSettingsActionType.ENABLE_SPELL_CHECK_GLOBALLY :
            LanguageSettingsActionType.DISABLE_SPELL_CHECK_GLOBALLY);
  }

  private onSelectedSpellingServiceChange_() {
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        this.prefs.spellcheck.use_spelling_service.value ?
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
    return isEnabled ? this.get('spellcheck.forced_dictionaries', this.prefs) :
                       this.get('spellcheck.blocked_dictionaries', this.prefs);
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
      this.setPrefValue('browser.enable_spellchecking', false);
    }

    if (this.spellCheckLanguages_.length === 1) {
      const singleLanguage = this.spellCheckLanguages_[0];

      // Hide list of spell check languages if there is only 1 language
      // and we don't need to display any errors for that language

      // TODO(crbug.com/40147587): Make hideSpellCheckLanugages_ a computed property
      this.hideSpellCheckLanguages_ = !singleLanguage.isManaged &&
          singleLanguage.downloadDictionaryFailureCount === 0;
    } else {
      this.hideSpellCheckLanguages_ = false;
    }
  }

  private updateSpellcheckEnabled_() {
    if (this.prefs === undefined) {
      return;
    }

    // If there is only 1 language, we hide the list of languages so users
    // are unable to toggle on/off spell check specifically for the 1
    // language. Therefore, we need to treat the toggle for
    // `browser.enable_spellchecking` as the toggle for the 1 language as
    // well.
    if (this.spellCheckLanguages_.length === 1) {
      this.languageHelper.toggleSpellCheck(
          this.spellCheckLanguages_[0].language.code,
          !!this.getPref('browser.enable_spellchecking').value);
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

    this.languageHelper.toggleSpellCheck(
        item.language.code, !item.spellCheckEnabled);

    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        item.spellCheckEnabled ?
            LanguageSettingsActionType.ENABLE_SPELL_CHECK_FOR_LANGUAGE :
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
    this.languageHelper.retryDownloadDictionary(e.model.item.language.code);
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
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-spell-check-page': SettingsSpellCheckPageElement;
  }
}

customElements.define(
    SettingsSpellCheckPageElement.is, SettingsSpellCheckPageElement);
