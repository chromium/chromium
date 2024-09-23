// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons_lit.html.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import './icons.html.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {toastDurationMs, ToolbarEvent} from './common.js';
import {getCss} from './language_menu.css.js';
import {getHtml} from './language_menu.html.js';
import {AVAILABLE_GOOGLE_TTS_LOCALES, getVoicePackConvertedLangIfExists, NotificationType} from './voice_language_util.js';
import type {VoiceNotificationListener} from './voice_notification_manager.js';
import {VoiceNotificationManager} from './voice_notification_manager.js';

export interface LanguageMenuElement {
  $: {
    languageMenu: CrDialogElement,
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
      lastDownloadedLang: {type: String},
      languageSearchValue_: {type: String},
      currentNotifications_: {type: Object},
      toastTitle_: {type: String},
      availableLanguages_: {type: Array},
    };
  }

  constructor() {
    super();
    this.notificationManager_.addListener(this);
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

    if (changedProperties.has('lastDownloadedLang')) {
      this.toastTitle_ = this.getLanguageDownloadedTitle_();
    }
  }

  notify(language: string, type: NotificationType) {
    this.currentNotifications_ = {
      ...this.currentNotifications_,
      [language]: type,
    };
  }

  selectedLang: string;
  localeToDisplayName: {[lang: string]: string} = {};
  enabledLangs: string[] = [];
  lastDownloadedLang: string;

  availableVoices: SpeechSynthesisVoice[];
  protected languageSearchValue_: string = '';
  protected toastTitle_: string = '';
  protected toastDuration_: number = toastDurationMs;
  protected availableLanguages_: LanguageDropdownItem[] = [];
  // Use this variable instead of AVAILABLE_GOOGLE_TTS_LOCALES
  // directly to better aid in testing.
  localesOfLangPackVoices: Set<string> =
      this.getSupportedNaturalVoiceDownloadLocales();

  // The current notifications that should be used in the language menu.
  private currentNotifications_: {[language: string]: NotificationType} = {};
  private notificationManager_: VoiceNotificationManager =
      VoiceNotificationManager.getInstance();

  protected closeLanguageMenu_() {
    this.notificationManager_.removeListener(this);
    this.$.languageMenu.close();
  }

  protected onClearSearchClick_() {
    this.languageSearchValue_ = '';
  }

  protected onToggleChange_(e: Event) {
    const index =
        Number.parseInt((e.currentTarget as HTMLElement).dataset['index']!);
    const language = this.availableLanguages_[index].languageCode;

    this.fire(ToolbarEvent.LANGUAGE_TOGGLE, {language});
  }

  private getDisplayName(lang: string) {
    const langLower = lang.toLowerCase();
    return this.localeToDisplayName[langLower] || langLower;
  }

  private getLanguageDownloadedTitle_() {
    if (!this.lastDownloadedLang) {
      return '';
    }
    const langDisplayName = this.getDisplayName(this.lastDownloadedLang);
    return loadTimeData.getStringF(
        'readingModeVoiceDownloadedTitle', langDisplayName);
  }

  private getSupportedNaturalVoiceDownloadLocales(): Set<string> {
    if (chrome.readingMode.isLanguagePackDownloadingEnabled &&
        chrome.readingMode.isChromeOsAsh) {
      return AVAILABLE_GOOGLE_TTS_LOCALES;
    }
    return new Set([]);
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
            // Check whether the search term matches the readable lang (e.g.
            // 'ras' will match 'Portugues (Brasil)'), and also if it matches
            // the language code (e.g. 'pt-br' matches 'Portugues (Brasil)')
            lang => isSubstring(
                        /* value= */ this.getDisplayName(lang),
                        /* substring= */ this.languageSearchValue_) ||
                isSubstring(
                        /* value= */ lang,
                        /* substring= */ this.languageSearchValue_))
        .map(lang => ({
               readableLanguage: this.getDisplayName(lang),
               checked: this.enabledLangs.includes(lang),
               languageCode: lang,
               notification: this.getNotificationFor(lang),
               disabled: this.enabledLangs.includes(lang) &&
                   (lang.toLowerCase() === selectedLangLowerCase),
             }));
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
      case NotificationType.NONE:
        return {isError: false};
      default:
        // This ensures the switch statement is exhaustive
        return notification satisfies never;
    }
  }

  // Runtime errors were thrown when this.i18n() was called in a Polymer
  // computed bindining callback function, so instead we call this.i18n from the
  // html via a wrapper.
  protected i18nWraper(s: string|undefined): string {
    return s ? this.i18n(s) : '';
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
