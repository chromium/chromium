// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-translate-page' is the settings page
 * translate settings.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './add_languages_dialog.js';
import './languages.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_shared.css.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';

import {LanguageSettingsActionType, LanguageSettingsMetricsProxy, LanguageSettingsMetricsProxyImpl} from './languages_settings_metrics_proxy.js';
import {LanguageHelper, LanguagesModel} from './languages_types.js';
import {getTemplate} from './translate_page.html.js';

const SettingsTranslatePageElementBase = PrefsMixin(PolymerElement);

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
  private onAlwaysTranslateLanguagesAdded_(e: CustomEvent<string[]>) {
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

  private onNeverTranslateLanguagesAdded_(e: CustomEvent<string[]>) {
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

  private onTranslateToggleChange_(e: Event) {
    this.languageSettingsMetricsProxy_.recordSettingsMetric(
        (e.target as SettingsToggleButtonElement).checked ?
            LanguageSettingsActionType.ENABLE_TRANSLATE_GLOBALLY :
            LanguageSettingsActionType.DISABLE_TRANSLATE_GLOBALLY);
  }

  /**
   * @return Whether the list is non-null and has items.
   */
  private hasSome_(list: any[]): boolean {
    return !!(list && list.length);
  }

  /**
   * Gets the list of languages that chrome can translate
   */
  private getTranslatableLanguages_():
      chrome.languageSettingsPrivate.Language[] {
    return this.languages!.supported.filter(language => {
      return this.languageHelper.isLanguageTranslatable(language);
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-translate-page': SettingsTranslatePageElement;
  }
}

customElements.define(
    SettingsTranslatePageElement.is, SettingsTranslatePageElement);
