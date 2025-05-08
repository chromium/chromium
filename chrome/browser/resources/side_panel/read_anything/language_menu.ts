// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import './language_toast.js';
import './icons.html.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolbarEvent} from './common.js';
import {getCss} from './language_menu.css.js';
import {getHtml} from './language_menu.html.js';
import type {LanguageToastElement} from './language_toast.js';
import {AVAILABLE_GOOGLE_TTS_LOCALES, getVoicePackConvertedLangIfExists, NotificationType} from './voice_language_util.js';
import type {VoiceNotificationListener} from './voice_notification_manager.js';
import {VoiceNotificationManager} from './voice_notification_manager.js';

export interface LanguageMenuElement {
  $: {
    languageMenu: CrDialogElement,
    searchField: CrInputElement,
  };
}

interface Notification {
  isError: boolean;
  text?: string;
}

interface LanguageDropdownItem {
  readableLanguage: string;
  checked: boolean;
  languageCode: string;
  notification: Notification;
  // Whether this toggle should be disabled
  disabled: boolean;
}

// Returns whether `substring` is a non-case-sensitive substring of `value`
function isSubstring(value: string, substring: string): boolean {
  return value.toLowerCase().includes(substring.toLowerCase());
}

const LanguageMenuElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class LanguageMenuElement extends LanguageMenuElementBase implements
    VoiceNotificationListener {
  static get is() {
    return 'language-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      enabledLangs: {type: Array},
      availableVoices: {type: Array},
      localeToDisplayName: {type: Object},
      selectedLang: {type: String},
      languageSearchValue_: {type: String},
      currentNotifications_: {type: Object},
      availableLanguages_: {type: Array},
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.notificationManager_.addListener(this);
    this.notificationManager_.addListener(this.getToast_());
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('selectedLang') ||
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
  // Use this variable instead of AVAILABLE_GOOGLE_TTS_LOCALES
  // directly to better aid in testing.
  localesOfLangPackVoices: Set<string> =
      this.getSupportedNaturalVoiceDownloadLocales();

  // The current notifications that should be used in the language menu.
  private accessor currentNotifications_:
      {[language: string]: NotificationType} = {};
  private notificationManager_: VoiceNotificationManager =
      VoiceNotificationManager.getInstance();

  protected closeLanguageMenu_() {
    this.notificationManager_.removeListener(this);
    this.notificationManager_.removeListener(this.getToast_());
    this.$.languageMenu.close();
  }

  protected onClearSearchClick_() {
    this.languageSearchValue_ = '';
    this.$.searchField.focus();
  }

  protected onToggleChange_(e: Event) {
    const index =
        Number.parseInt((e.currentTarget as HTMLElement).dataset['index']!);
    const language = this.availableLanguages_[index]!.languageCode;

    this.fire(ToolbarEvent.LANGUAGE_TOGGLE, {language});
  }

  private getToast_(): LanguageToastElement {
    const toast = this.$.languageMenu.querySelector<LanguageToastElement>(
        'language-toast');
    assert(toast, 'no language menu toast!');
    return toast;
  }

  private getDisplayName(lang: string) {
    const langLower = lang.toLowerCase();
    return this.localeToDisplayName[langLower] || langLower;
  }

  private getNormalizedDisplayName(lang: string) {
    const displayName = this.getDisplayName(lang);
    return displayName.normalize('NFD').replace(/[\u0300-\u036f]/g, '');
  }

  private getSupportedNaturalVoiceDownloadLocales(): Set<string> {
    return AVAILABLE_GOOGLE_TTS_LOCALES;
  }

  private computeAvailableLanguages_(): LanguageDropdownItem[] {
    if (!this.availableVoices) {
      return [];
    }

    const selectedLangLowerCase = this.selectedLang?.toLowerCase();

    const availableLangs: string[] = [...new Set([
      ...this.localesOfLangPackVoices,
      ...this.availableVoices.map(({lang}) => lang.toLowerCase()),
    ])];

    // Sort the list of languages alphabetically by display name.
    availableLangs.sort((lang1, lang2) => {
      return this.getDisplayName(lang1).localeCompare(
          this.getDisplayName(lang2));
    });

    return availableLangs
        .filter(
            lang => this.isLanguageSearchMatch(lang, this.languageSearchValue_))
        .map(lang => ({
               readableLanguage: this.getDisplayName(lang),
               checked: this.enabledLangs.includes(lang),
               languageCode: lang,
               notification: this.getNotificationFor(lang),
               disabled: this.enabledLangs.includes(lang) &&
                   (lang.toLowerCase() === selectedLangLowerCase),
             }));
  }

  // Check whether the search term matches the readable lang (e.g.
  // 'ras' will match 'Portugues (Brasil)'), if it matches
  // the language code (e.g. 'pt-br' matches 'Portugues (Brasil)'), or if it
  // matches without accents (e.g. 'portugues' matches 'portugués').
  private isLanguageSearchMatch(lang: string, languageSearchValue: string):
      boolean {
    const isDisplayNameMatch = isSubstring(
        /* value= */ this.getDisplayName(lang),
        /* substring= */ languageSearchValue);
    const isLanguageCodeMatch = isSubstring(
        /* value= */ lang,
        /* substring= */ languageSearchValue);

    // Compare the search term to the language name without
    // accents.
    const isNormalizedDisplayNameMatch = isSubstring(
        /* value= */ this.getNormalizedDisplayName(lang),
        /* substring= */ languageSearchValue);

    return isDisplayNameMatch || isLanguageCodeMatch ||
        isNormalizedDisplayNameMatch;
  }

  private getNotificationFor(lang: string): Notification {
    const voicePackLanguage = getVoicePackConvertedLangIfExists(lang);
    const notification = this.currentNotifications_[voicePackLanguage];
    if (notification === undefined) {
      return {isError: false};
    }

    switch (notification) {
      case NotificationType.DOWNLOADING:
        return {isError: false, text: 'readingModeLanguageMenuDownloading'};
      case NotificationType.NO_INTERNET:
        return {isError: true, text: 'readingModeLanguageMenuNoInternet'};
      case NotificationType.GENERIC_ERROR:
        return {isError: true, text: 'languageMenuDownloadFailed'};
      case NotificationType.NO_SPACE_HQ:
        return {isError: true, text: 'allocationErrorHighQuality'};
      case NotificationType.NO_SPACE:
        return {isError: true, text: 'allocationError'};
      case NotificationType.DOWNLOADED:
      case NotificationType.GOOGLE_VOICES_UNAVAILABLE:
        // TODO (crbug.com/396436665) Show inline error message
      case NotificationType.NONE:
        return {isError: false};
      default:
        // This ensures the switch statement is exhaustive
        return notification satisfies never;
    }
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
}

declare global {
  interface HTMLElementTagNameMap {
    'language-menu': LanguageMenuElement;
  }
}

customElements.define(LanguageMenuElement.is, LanguageMenuElement);
