// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-languages-page' is the settings page
 * for language and input method settings.
 */
// clang-format off

import 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './add_languages_dialog.js';
import '../icons.html.js';
import '../relaunch_confirmation_dialog.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert} from 'chrome://resources/js/assert.js';
import {isWindows} from 'chrome://resources/js/platform.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import { PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// <if expr="is_win">
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// </if>

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';

import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import { RouteObserverMixin} from '../router.js';

import {getTemplate} from './languages_page.html.js';
import type { LanguageSettingsMetricsProxy} from './languages_settings_metrics_proxy.js';
import {LanguageSettingsActionType, LanguageSettingsMetricsProxyImpl, LanguageSettingsPageImpressionType} from './languages_settings_metrics_proxy.js';
import type {LanguageHelper, LanguagesModel, LanguageState} from './languages_types.js';

// clang-format on

/**
 * Millisecond delay that can be used when closing an action menu to keep it
 * briefly on-screen.
 */
export const kMenuCloseDelay: number = 100;

export interface SettingsLanguagesPageElement {
  $: {
    menu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

const SettingsLanguagesPageElementBase =
    RouteObserverMixin(RelaunchMixin(I18nMixin(PrefsMixin(PolymerElement))));

export class SettingsLanguagesPageElement extends
    SettingsLanguagesPageElementBase {
  static get is() {
    return 'settings-languages-page';
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

      /**
       * The language to display the details for.
       */
      detailLanguage_: Object,

      showAddLanguagesDialog_: Boolean,
      addLanguagesDialogLanguages_: Array,

      showManagedLanguageDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  languages?: LanguagesModel;
  languageHelper: LanguageHelper;
  private detailLanguage_?: LanguageState;
  private showAddLanguagesDialog_: boolean;
  private addLanguagesDialogLanguages_:
      chrome.languageSettingsPrivate.Language[]|null;
  private showManagedLanguageDialog_: boolean;
  private languageSettingsMetricsProxy_: LanguageSettingsMetricsProxy =
      LanguageSettingsMetricsProxyImpl.getInstance();

  // <if expr="is_win">
  private isChangeInProgress_: boolean = false;
  // </if>

  /**
   * Stamps and opens the Add Languages dialog, registering a listener to
   * disable the dialog's dom-if again on close.
   */
  private onAddLanguagesClick_(e: Event) {
    e.preventDefault();
    this.languageSettingsMetricsProxy_.recordPageImpressionMetric(
        LanguageSettingsPageImpressionType.ADD_LANGUAGE);

    this.addLanguagesDialogLanguages_ = this.languages!.supported.filter(
        language => this.languageHelper.canEnableLanguage(language));
    this.showAddLanguagesDialog_ = true;
  }

  private onLanguagesAdded_(e: CustomEvent<string[]>) {
    const languagesToAdd = e.detail;
    languagesToAdd.forEach(languageCode => {
      this.languageHelper.enableLanguage(languageCode);
      LanguageSettingsMetricsProxyImpl.getInstance().recordSettingsMetric(
          LanguageSettingsActionType.LANGUAGE_ADDED);
    });
  }

  private onAddLanguagesDialogClose_() {
    this.showAddLanguagesDialog_ = false;
    this.addLanguagesDialogLanguages_ = null;
    const toFocus =
        this.shadowRoot!.querySelector<HTMLElement>('#addLanguages');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  /**
   * Formats language index (zero-indexed)
   */
  private formatIndex_(index: number): string {
    return (index + 1).toLocaleString();
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
   * @param languageCode The language code identifying a language.
   * @param translateTarget The target language.
   * @return 'target' if |languageCode| matches the target language,
   *     'non-target' otherwise.
   */
  private isTranslationTarget_(languageCode: string, translateTarget: string):
      string {
    if (this.languageHelper.convertLanguageCodeForTranslate(languageCode) ===
        translateTarget) {
      return 'target';
    } else {
      return 'non-target';
    }
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
    const restartButton =
        this.shadowRoot!.querySelector<HTMLElement>('#restartButton');
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
  private disableUiLanguageCheckbox_(
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
  private onUiLanguageChange_(e: Event) {
    // We don't support unchecking this checkbox. TODO(michaelpg): Ask for a
    // simpler widget.
    assert((e.target as CrCheckboxElement).checked);
    this.isChangeInProgress_ = true;
    this.languageHelper.setProspectiveUiLanguage(
        this.detailLanguage_!.language.code);
    this.languageHelper.moveLanguageToFront(
        this.detailLanguage_!.language.code);
    LanguageSettingsMetricsProxyImpl.getInstance().recordSettingsMetric(
        LanguageSettingsActionType.CHANGE_CHROME_LANGUAGE);

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
  private isProspectiveUiLanguage_(
      languageCode: string, prospectiveUILanguage: string): boolean {
    return languageCode === prospectiveUILanguage;
  }

  /**
   * Handler for the restart button.
   */
  private onRestartClick_() {
    this.performRestart(RestartType.RESTART);
  }
  // </if>

  /**
   * Moves the language to the top of the list.
   */
  private onMoveToTopClick_() {
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
  private onMoveUpClick_() {
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
  private onMoveDownClick_() {
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
  private onRemoveLanguageClick_() {
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

  private onDotsClick_(e: DomRepeatEvent<LanguageState>) {
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

  override currentRouteChanged(currentRoute: Route) {
    if (currentRoute === routes.LANGUAGES) {
      this.languageSettingsMetricsProxy_.recordPageImpressionMetric(
          LanguageSettingsPageImpressionType.MAIN);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-languages-page': SettingsLanguagesPageElement;
  }
}

customElements.define(
    SettingsLanguagesPageElement.is, SettingsLanguagesPageElement);
