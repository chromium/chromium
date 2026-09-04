// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import './language_toast.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolbarEvent} from '../content/read_anything_types.js';
import type {LanguageDropdownItem} from '../menus/menu_util.js';
import {ReadAloudSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import {getCss} from './accent_menu.css.js';
import {getHtml} from './accent_menu.html.js';
import {getDisplayName, isLanguageSearchMatch, sortLanguagesByDisplayName} from './language_display.js';
import type {LanguageToastElement} from './language_toast.js';
import type {NotificationType} from './voice_language_conversions.js';
import {AVAILABLE_GOOGLE_TTS_LOCALES, getNotificationFor} from './voice_language_conversions.js';
import type {VoiceNotificationListener} from './voice_notification_manager.js';
import {VoiceNotificationManager} from './voice_notification_manager.js';

export interface AccentMenuElement {
  $: {
    accentMenu: CrDialogElement,
    searchField: CrInputElement,
  };
}

const AccentMenuElementBase = WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class AccentMenuElement extends AccentMenuElementBase implements
    VoiceNotificationListener {
  static get is() {
    return 'accent-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      availableVoices: {type: Array},
      enabledLangs: {type: Array},
      localeToDisplayName: {type: Object},
      selectedLang: {type: String},
      languageSearchValue_: {type: String},
      currentNotifications_: {type: Object},
      availableLanguages_: {type: Array},
      webuiRoundedIconsEnabled_: {type: Boolean},
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.notificationManager_.addListener(this);
    this.notificationManager_.addListener(this.getToast_());
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.notificationManager_.removeListener(this);
    const toast = this.getToast_();
    if (toast) {
      this.notificationManager_.removeListener(toast);
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('selectedLang') ||
        changedProperties.has('availableVoices') ||
        changedProperties.has('enabledLangs') ||
        changedProperties.has('localeToDisplayName') ||
        changedPrivateProperties.has('currentNotifications_') ||
        changedPrivateProperties.has('languageSearchValue_')) {
      this.availableLanguages_ = this.computeAvailableLanguages_();
    }
  }

  notify(type: NotificationType, language?: string) {
    if (!language) {
      return;
    }
    this.currentNotifications_ = {
      ...this.currentNotifications_,
      [language]: type,
    };
  }

  accessor selectedLang: string = '';
  accessor localeToDisplayName: {[lang: string]: string} = {};
  accessor enabledLangs: string[] = [];
  accessor availableVoices: SpeechSynthesisVoice[] = [];
  protected accessor languageSearchValue_: string = '';
  protected accessor availableLanguages_: LanguageDropdownItem[] = [];
  protected accessor webuiRoundedIconsEnabled_: boolean =
      loadTimeData.getBoolean('webuiRoundedIconsEnabled');
  localesOfLangPackVoices: Set<string> = AVAILABLE_GOOGLE_TTS_LOCALES;

  // The current notifications that should be used in the accent menu.
  private accessor currentNotifications_:
      {[language: string]: NotificationType} = {};
  private notificationManager_: VoiceNotificationManager =
      VoiceNotificationManager.getInstance();
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  protected onClose_() {
    this.notificationManager_.removeListener(this);
    this.notificationManager_.removeListener(this.getToast_());
    this.$.accentMenu.close();
  }

  protected onClearSearchClick_() {
    this.languageSearchValue_ = '';
    this.$.searchField.focus();
  }

  protected onLanguageSelectClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const index = Number.parseInt(target.dataset['index'] || '', 10);
    const item = this.availableLanguages_[index];
    if (!item) {
      return;
    }

    this.logger_.logSpeechSettingsChange(ReadAloudSettingsChange.ACCENT_CHANGE);
    this.fire(ToolbarEvent.LANGUAGE_SELECTED, {language: item.languageCode});
  }

  protected getItemAriaChecked_(item: LanguageDropdownItem): boolean {
    return item.selected ?? false;
  }

  private getToast_(): LanguageToastElement {
    const toast =
        this.$.accentMenu.querySelector<LanguageToastElement>('language-toast');
    assert(toast, 'no accent menu toast!');
    return toast;
  }

  private computeAvailableLanguages_(): LanguageDropdownItem[] {
    if (!this.availableVoices) {
      return [];
    }

    const selectedLangLowerCase = this.selectedLang.toLowerCase();
    const availableLangs = [...new Set([
      ...this.localesOfLangPackVoices,
      ...this.availableVoices.map(({lang}) => lang.toLowerCase()),
    ])];

    sortLanguagesByDisplayName(availableLangs, this.localeToDisplayName);

    return availableLangs
        .filter(
            lang => isLanguageSearchMatch(
                lang, this.languageSearchValue_, this.localeToDisplayName))
        .map(lang => ({
               readableLanguage: getDisplayName(lang, this.localeToDisplayName),
               languageCode: lang,
               notification:
                   getNotificationFor(lang, this.currentNotifications_),
               selected: lang.toLowerCase() === selectedLangLowerCase,
             }));
  }

  protected searchHasLanguages(): boolean {
    // We should only show the "No results" string when there are no available
    // languages and there is a valid search term.
    return (this.availableLanguages_.length > 0) ||
        (!this.languageSearchValue_) ||
        (this.languageSearchValue_.trim().length === 0);
  }

  protected onLanguageSearchValueChanged_(e: CustomEvent<{value: string}>) {
    this.languageSearchValue_ = e.detail.value;
  }

  protected onKeydown_(e: KeyboardEvent) {
    e.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accent-menu': AccentMenuElement;
  }
}

customElements.define(AccentMenuElement.is, AccentMenuElement);
