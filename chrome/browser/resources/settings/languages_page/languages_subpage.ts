// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-languages-page' is the settings page
 * for language and input method settings.
 */

import 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './add_languages_dialog.js';
import './languages.js';
import '../controls/settings_toggle_button.js';
import '../icons.js';
// <if expr="not chromeos_ash">
import '../relaunch_confirmation_dialog.js';
// </if>
import '../settings_shared_css.js';
import '../settings_vars_css.js';

import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {isWindows} from 'chrome://resources/js/cr.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// <if expr="is_win">
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// </if>

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';
import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';

import {LanguageSettingsActionType, LanguageSettingsMetricsProxy, LanguageSettingsMetricsProxyImpl, LanguageSettingsPageImpressionType} from './languages_settings_metrics_proxy.js';
import {getTemplate} from './languages_subpage.html.js';
import {LanguageHelper, LanguagesModel, LanguageState} from './languages_types.js';

/**
 * Millisecond delay that can be used when closing an action menu to keep it
 * briefly on-screen.
 */
export const kMenuCloseDelay: number = 100;

type FocusConfig = Map<string, (string|(() => void))>;

export interface SettingsLanguagesSubpageElement {
  $: {
    menu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

const SettingsLanguagesSubpageElementBase =
    RelaunchMixin(I18nMixin(PrefsMixin(PolymerElement)));

export class SettingsLanguagesSubpageElement extends
    SettingsLanguagesSubpageElementBase {
  static get is() {
    return 'settings-languages-subpage';
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

      /**
       * The language to display the details for.
       */
      detailLanguage_: Object,

      showAddLanguagesDialog_: Boolean,
      showAddAlwaysTranslateDialog_: Boolean,
      showAddNeverTranslateDialog_: Boolean,
      addLanguagesDialogLanguages_: Array,

      focusConfig_: {
        type: Object,
        value: function() {
          return new Map();
        },
      },

      showManagedLanguageDialog_: {
        type: Boolean,
        value: false,
      },

      enableDesktopDetailedLanguageSettings_: {
        type: Boolean,
        value: function() {
          let enabled = false;
          // <if expr="not chromeos_lacros">
          enabled =
              loadTimeData.getBoolean('enableDesktopDetailedLanguageSettings');
          // </if>
          return enabled;
        },
      },
    };
  }

  languages?: LanguagesModel;
  languageHelper: LanguageHelper;
  private detailLanguage_?: LanguageState;
  private showAddLanguagesDialog_: boolean;
  private showAddAlwaysTranslateDialog_: boolean;
  private showAddNeverTranslateDialog_: boolean;
  private addLanguagesDialogLanguages_:
      Array<chrome.languageSettingsPrivate.Language>|null;
  private focusConfig_: FocusConfig;
  private showManagedLanguageDialog_: boolean;
  private enableDesktopDetailedLanguageSettings_: boolean;
  private languageSettingsMetricsProxy_: LanguageSettingsMetricsProxy =
      LanguageSettingsMetricsProxyImpl.getInstance();

  // <if expr="is_win">
  private isChangeInProgress_: boolean = false;
  // </if>

  /**
   * Stamps and opens the Add Languages dialog, registering a listener to
   * disable the dialog's dom-if again on close.
   */
  private onAddLanguagesTap_(e: Event) {
    e.preventDefault();
    this.languageSettingsMetricsProxy_.recordPageImpressionMetric(
        LanguageSettingsPageImpressionType.ADD_LANGUAGE);

    this.addLanguagesDialogLanguages_ = this.languages!.supported.filter(
        language => this.languageHelper.canEnableLanguage(language));
    this.showAddLanguagesDialog_ = true;
  }

  private onAddLanguagesDialogClose_() {
    this.showAddLanguagesDialog_ = false;
    this.addLanguagesDialogLanguages_ = null;
    const toFocus =
        this.shadowRoot!.querySelector<HTMLElement>('#addLanguages');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  private onLanguagesAdded_(e: CustomEvent<Array<string>>) {
    const languagesToAdd = e.detail;
    languagesToAdd.forEach(languageCode => {
      this.languageHelper.enableLanguage(languageCode);
      LanguageSettingsMetricsProxyImpl.getInstance().recordSettingsMetric(
          LanguageSettingsActionType.LANGUAGE_ADDED);
    });
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
    const toFocus = this.shadowRoot!.querySelector('#addAlwaysTranslate');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  /**
   * Helper function fired by the add dialog's on-languages-added event. Adds
   * selected languages to the always-translate languages list.
   */
  private onAlwaysTranslateLanguagesAdded_(e: CustomEvent<Array<string>>) {
    const languagesToAdd = e.detail;
    languagesToAdd.forEach(languageCode => {
      this.languageHelper.setLanguageAlwaysTranslateState(languageCode, true);
    });
  }

  /**
   * Removes a language from the always translate languages list.
   */
  private onRemoveAlwaysTranslateLanguageClick_(
      e: DomRepeatEvent<chrome.languageSettingsPrivate.Language>) {
    const languageCode = e.model.item.code;
    this.languageHelper.setLanguageAlwaysTranslateState(languageCode, false);
  }

  /**
   * Checks if there are supported languages that are not enabled but can be
   * enabled.
   * @return True if there is at least one available language.
   */
  private canEnableSomeSupportedLanguage_(languages?: LanguagesModel): boolean {
    return languages === undefined || languages.supported.some(language => {
      return this.languageHelper.canEnableLanguage(language);
    });
  }

  /**
   * Stamps and opens the Add Languages dialog, registering a listener to
   * disable the dialog's dom-if again on close.
   */
  private onAddNeverTranslateLanguagesClick_(e: Event) {
    e.preventDefault();

    this.addLanguagesDialogLanguages_ = this.languages!.supported.filter(
        language => !this.languages!.neverTranslate.includes(language));
    this.showAddNeverTranslateDialog_ = true;
  }

  private onNeverTranslateDialogClose_() {
    this.showAddNeverTranslateDialog_ = false;
    this.addLanguagesDialogLanguages_ = null;
    const toFocus = this.shadowRoot!.querySelector('#addNeverTranslate');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  private onNeverTranslateLanguagesAdded_(e: CustomEvent<Array<string>>) {
    const languagesToAdd = e.detail;
    languagesToAdd.forEach(languageCode => {
      this.languageHelper.disableTranslateLanguage(languageCode);
    });
  }

  /**
   * Removes a language from the never translate languages list.
   */
  private onRemoveNeverTranslateLanguageClick_(
      e: DomRepeatEvent<chrome.languageSettingsPrivate.Language>) {
    const languageCode = e.model.item.code;
    this.languageHelper.enableTranslateLanguage(languageCode);
  }

  /**
   * Used to determine whether to show the separator between checkbox settings
   * and move buttons in the dialog menu.
   * @return True if there is currently more than one selected language.
   */
  private shouldShowDialogSeparator_(): boolean {
    return this.languages !== undefined && this.languages.enabled.length > 1;
  }

  /**
   * Used to determine which "Move" buttons to show for ordering enabled
   * languages.
   * @return True if |language| is at the |n|th index in the list of enabled
   *     languages.
   */
  private isNthLanguage_(n: number): boolean {
    if (this.languages === undefined || this.detailLanguage_ === undefined) {
      return false;
    }

    if (n >= this.languages.enabled.length) {
      return false;
    }

    const compareLanguage = this.languages.enabled[n]!;
    return this.detailLanguage_.language === compareLanguage.language;
  }

  /**
   * @return True if the "Move to top" option for |language| should be visible.
   */
  private showMoveUp_(): boolean {
    // "Move up" is a no-op for the top language, and redundant with
    // "Move to top" for the 2nd language.
    return !this.isNthLanguage_(0) && !this.isNthLanguage_(1);
  }

  /**
   * @return True if the "Move down" option for |language| should be visible.
   */
  private showMoveDown_(): boolean {
    return this.languages !== undefined &&
        !this.isNthLanguage_(this.languages.enabled.length - 1);
  }

  /**
   * @return True if there are less than 2 languages.
   */
  private isHelpTextHidden_(): boolean {
    return this.languages !== undefined && this.languages.enabled.length <= 1;
  }

  /**
   * @param languageCode The language code identifying a language.
   * @param translateTarget The target language.
   * @return 'target' if |languageCode| matches the target language,
   *     'non-target' otherwise.
   */
  isTranslationTarget_(languageCode: string, translateTarget: string): string {
    if (this.languageHelper.convertLanguageCodeForTranslate(languageCode) ===
        translateTarget) {
      return 'target';
    } else {
      return 'non-target';
    }
  }

  private onTranslateToggleChange_(e: Event) {
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        (e.target as SettingsToggleButtonElement).checked ?
            LanguageSettingsActionType.ENABLE_TRANSLATE_GLOBALLY :
            LanguageSettingsActionType.DISABLE_TRANSLATE_GLOBALLY);
  }

  // <if expr="is_win">
  /**
   * @param languageCode The language code identifying a language.
   * @param prospectiveUILanguage The prospective UI language.
   * @return True if the prospective UI language is set to
   *     |languageCode| but requires a restart to take effect.
   */
  private isRestartRequired_(
      languageCode: string, prospectiveUILanguage: string): boolean {
    return prospectiveUILanguage === languageCode &&
        this.languageHelper.requiresRestart();
  }

  private onCloseMenu_() {
    if (!this.isChangeInProgress_) {
      return;
    }
    flush();
    this.isChangeInProgress_ = false;
    const restartButton = this.shadowRoot!.querySelector('#restartButton');
    if (!restartButton) {
      return;
    }
    focusWithoutInk(restartButton);
  }

  /**
   * @param prospectiveUILanguage The chosen UI language.
   * @return True if the given language cannot be set as the
   *     prospective UI language by the user.
   */
  private disableUILanguageCheckbox_(
      languageState: LanguageState, prospectiveUILanguage: string): boolean {
    if (this.detailLanguage_ === undefined) {
      return true;
    }

    // If the language cannot be a UI language, we can't set it as the
    // prospective UI language.
    if (!languageState.language.supportsUI) {
      return true;
    }

    // Unchecking the currently chosen language doesn't make much sense.
    if (languageState.language.code === prospectiveUILanguage) {
      return true;
    }

    // Check if the language is prohibited by the current "AllowedLanguages"
    // policy.
    if (languageState.language.isProhibitedLanguage) {
      return true;
    }

    // Otherwise, the prospective language can be changed to this language.
    return false;
  }

  /**
   * Handler for changes to the UI language checkbox.
   */
  private onUILanguageChange_(e: Event) {
    // We don't support unchecking this checkbox. TODO(michaelpg): Ask for a
    // simpler widget.
    assert((e.target as CrCheckboxElement).checked);
    this.isChangeInProgress_ = true;
    this.languageHelper.setProspectiveUILanguage(
        this.detailLanguage_!.language.code);
    this.languageHelper.moveLanguageToFront(
        this.detailLanguage_!.language.code);

    this.closeMenuSoon_();
  }

  /**
   * Checks whether the prospective UI language (the pref that indicates what
   * language to use in Chrome) matches the current language. This pref is
   * used only on Chrome OS and Windows; we don't control the UI language
   * elsewhere.
   * @param languageCode The language code identifying a language.
   * @param prospectiveUILanguage The prospective UI language.
   * @return True if the given language matches the prospective UI pref (which
   *     may be different from the actual UI language).
   */
  private isProspectiveUILanguage_(
      languageCode: string, prospectiveUILanguage: string): boolean {
    return languageCode === prospectiveUILanguage;
  }

  /**
   * Handler for the restart button.
   */
  private onRestartTap_() {
    this.performRestart(RestartType.RESTART);
  }
  // </if>

  /**
   * @param targetLanguageCode The default translate target language.
   * @return True if the translate checkbox should be disabled.
   */
  private disableTranslateCheckbox_(
      languageState: LanguageState|undefined,
      targetLanguageCode: string): boolean {
    if (languageState === undefined || languageState.language === undefined ||
        !languageState.language.supportsTranslate) {
      return true;
    }

    if (this.languageHelper.isOnlyTranslateBlockedLanguage(languageState)) {
      return true;
    }

    return this.languageHelper.convertLanguageCodeForTranslate(
               languageState.language.code) === targetLanguageCode;
  }

  /**
   * Handler for changes to the translate checkbox.
   */
  private onTranslateCheckboxChange_(e: Event) {
    if ((e.target as CrCheckboxElement).checked) {
      this.languageHelper.enableTranslateLanguage(
          this.detailLanguage_!.language.code);

      this.languageSettingsMetricsProxy_.recordSettingsMetric(
          LanguageSettingsActionType.ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE);

    } else {
      this.languageHelper.disableTranslateLanguage(
          this.detailLanguage_!.language.code);

      this.languageSettingsMetricsProxy_.recordSettingsMetric(
          LanguageSettingsActionType.DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE);
    }
    this.closeMenuSoon_();
  }

  /**
   * Returns "complex" if the menu includes checkboxes, which should change
   * the spacing of items and show a separator in the menu.
   */
  getMenuClass_(translateEnabled: boolean): string {
    if (translateEnabled || isWindows) {
      return 'complex';
    }
    return '';
  }

  /**
   * Moves the language to the top of the list.
   */
  private onMoveToTopTap_() {
    this.$.menu.get().close();
    if (this.detailLanguage_!.isForced) {
      // If language is managed, show dialog to inform user it can't be modified
      this.showManagedLanguageDialog_ = true;
      return;
    }
    this.languageHelper.moveLanguageToFront(
        this.detailLanguage_!.language.code);
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        LanguageSettingsActionType.LANGUAGE_LIST_REORDERED);
  }

  /**
   * Moves the language up in the list.
   */
  private onMoveUpTap_() {
    this.$.menu.get().close();
    if (this.detailLanguage_!.isForced) {
      // If language is managed, show dialog to inform user it can't be modified
      this.showManagedLanguageDialog_ = true;
      return;
    }
    this.languageHelper.moveLanguage(
        this.detailLanguage_!.language.code, true /* upDirection */);
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        LanguageSettingsActionType.LANGUAGE_LIST_REORDERED);
  }

  /**
   * Moves the language down in the list.
   */
  private onMoveDownTap_() {
    this.$.menu.get().close();
    if (this.detailLanguage_!.isForced) {
      // If language is managed, show dialog to inform user it can't be modified
      this.showManagedLanguageDialog_ = true;
      return;
    }
    this.languageHelper.moveLanguage(
        this.detailLanguage_!.language.code, false /* upDirection */);
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        LanguageSettingsActionType.LANGUAGE_LIST_REORDERED);
  }

  /**
   * Disables the language.
   */
  private onRemoveLanguageTap_() {
    this.$.menu.get().close();
    if (this.detailLanguage_!.isForced) {
      // If language is managed, show dialog to inform user it can't be modified
      this.showManagedLanguageDialog_ = true;
      return;
    }
    this.languageHelper.disableLanguage(this.detailLanguage_!.language.code);
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        LanguageSettingsActionType.LANGUAGE_REMOVED);
  }

  /**
   * Returns either the "selected" class, if the language matches the
   * prospective UI language, or an empty string. Languages can only be
   * selected on Chrome OS and Windows.
   * @param languageCode The language code identifying a language.
   * @param prospectiveUILanguage The prospective UI language.
   * @return The class name for the language item.
   */
  private getLanguageItemClass_(
      languageCode: string, prospectiveUILanguage: string): string {
    if (isWindows && languageCode === prospectiveUILanguage) {
      return 'selected';
    }
    return '';
  }

  private onDotsTap_(e: DomRepeatEvent<LanguageState>) {
    // Set a copy of the LanguageState object since it is not data-bound to
    // the languages model directly.
    this.detailLanguage_ = Object.assign({}, e.model.item);

    this.$.menu.get().showAt(e.target as HTMLElement);
    this.languageSettingsMetricsProxy_.recordPageImpressionMetric(
        LanguageSettingsPageImpressionType.LANGUAGE_OVERFLOW_MENU_OPENED);
  }

  /**
   * Closes the shared action menu after a short delay, so when a checkbox is
   * clicked it can be seen to change state before disappearing.
   */
  private closeMenuSoon_() {
    const menu = this.$.menu.get();
    setTimeout(function() {
      if (menu.open) {
        menu.close();
      }
    }, kMenuCloseDelay);
  }

  /**
   * Triggered when the managed language dialog is dismissed.
   */
  private onManagedLanguageDialogClosed_() {
    this.showManagedLanguageDialog_ = false;
  }

  /**
   * @return Whether the list is non-null and has items.
   */
  private hasSome_(list: Array<any>): boolean {
    return !!(list && list.length);
  }

  /**
   * Gets the list of languages that chrome can translate
   */
  private getTranslatableLanguages_():
      Array<chrome.languageSettingsPrivate.Language> {
    return this.languages!.supported.filter(language => {
      return this.languageHelper.isLanguageTranslatable(language);
    });
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-languages-subpage': SettingsLanguagesSubpageElement;
  }
}

customElements.define(
    SettingsLanguagesSubpageElement.is, SettingsLanguagesSubpageElement);
