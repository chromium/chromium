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
import {AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleForVoicePackManager, VoiceClientSideStatusCode} from './voice_language_util.js';

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

function isDownloading(voiceStatus: VoiceClientSideStatusCode) {
  switch (voiceStatus) {
    case VoiceClientSideStatusCode.SENT_INSTALL_REQUEST:
    case VoiceClientSideStatusCode.SENT_INSTALL_REQUEST_ERROR_RETRY:
    case VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE:
      return true;
    case VoiceClientSideStatusCode.AVAILABLE:
    case VoiceClientSideStatusCode.ERROR_INSTALLING:
    case VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION:
    case VoiceClientSideStatusCode.NOT_INSTALLED:
      return false;
    default:
      // This ensures the switch statement is exhaustive
      return voiceStatus satisfies never;
  }
}

// Returns whether `substring` is a non-case-sensitive substring of `value`
function isSubstring(value: string, substring: string): boolean {
  return value.toLowerCase().includes(substring.toLowerCase());
}

const LanguageMenuElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class LanguageMenuElement extends LanguageMenuElementBase {
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
      voicePackInstallStatus: {type: Object},
      selectedLang: {type: String},
      lastDownloadedLang: {type: String},
      languageSearchValue_: {type: String},
      currentNotifications_: {type: Array},
      toastTitle_: {type: String},
      availableLanguages_: {type: Array},
    };
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

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('voicePackInstallStatus')) {
      this.updateNotifications_(
          /* newVoiceStatuses= */ this.voicePackInstallStatus,
          /* oldVoiceStatuses= */
          changedProperties.get('voicePackInstallStatus'));
    }
  }

  selectedLang: string;
  localeToDisplayName: {[lang: string]: string} = {};
  enabledLangs: string[] = [];
  lastDownloadedLang: string;

  availableVoices: SpeechSynthesisVoice[];
  protected languageSearchValue_: string = '';
  protected toastTitle_: string = '';
  protected toastDuration_: number = toastDurationMs;
  voicePackInstallStatus: {[language: string]: VoiceClientSideStatusCode};
  protected availableLanguages_: LanguageDropdownItem[] = [];
  // Use this variable instead of AVAILABLE_GOOGLE_TTS_LOCALES
  // directly to better aid in testing.
  localesOfLangPackVoices: Set<string> =
      this.getSupportedNaturalVoiceDownloadLocales();

  // The current notifications that should be used in the language menu.
  // This is cleared each time the language menu reopens. After the language
  // menu reopens, only new changes to voicePackInstallStatus will be reflected
  // in notifications.
  private currentNotifications_:
      {[language: string]: VoiceClientSideStatusCode} = {};

  // Returns a copy of voicePackInstallStatus to use as a snapshot of the
  // current state. Before copying over the map, check the diff of
  // the new voicePackInstallStatus and our previous snapshot. If there are
  // any differences, add these to the currentNotifications_ map.
  private updateNotifications_(
      newVoiceStatuses: {[language: string]: VoiceClientSideStatusCode},
      oldVoiceStatuses?: {[language: string]: VoiceClientSideStatusCode}) {
    for (const lang of Object.keys(newVoiceStatuses)) {
      const newStatus = newVoiceStatuses[lang];
      // Since the downloading messages are cleared quickly, we should still
      // show "downloading" notifications, even if they were previously shown.
      if (isDownloading(newStatus)) {
        this.setNotification(lang, newStatus);
      } else if (oldVoiceStatuses && oldVoiceStatuses[lang] !== newStatus) {
        // Update the notification status for recently changed language keys.
        // Only show updates that occur while the language menu is open- don't
        // show notifications if updates occurred before the menu opened.
        this.setNotification(lang, newStatus);
      }
    }
  }

  private setNotification(lang: string, status: VoiceClientSideStatusCode) {
    this.currentNotifications_ = {
      ...this.currentNotifications_,
      [lang]: status,
    };
  }
  protected closeLanguageMenu_() {
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

  private hasAvailableNaturalVoices(lang: string): boolean {
    return this.localesOfLangPackVoices.has(lang.toLowerCase());
  }

  private getNotificationFor(lang: string): Notification {
    // Don't show notification text for a non-Google TTS language, as we're
    // not attempting a download.
    if (!this.hasAvailableNaturalVoices(lang)) {
      return {isError: false};
    }

    // Convert the lang code string to the language-pack format
    const voicePackLanguage = convertLangOrLocaleForVoicePackManager(lang);
    // No need to check the install status if the language is missing.
    if (!voicePackLanguage) {
      return {isError: false};
    }

    const notification = this.currentNotifications_[voicePackLanguage];
    if (notification === undefined) {
      return {isError: false};
    }

    // TODO(b/300259625): Show more error messages.
    switch (notification) {
      case VoiceClientSideStatusCode.SENT_INSTALL_REQUEST:
      case VoiceClientSideStatusCode.SENT_INSTALL_REQUEST_ERROR_RETRY:
      case VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE:
        return {isError: false, text: 'readingModeLanguageMenuDownloading'};
      case VoiceClientSideStatusCode.ERROR_INSTALLING:
        // There's not a specific error code from the language pack installer
        // for internet connectivity, but if there's an installation error
        // and we detect we're offline, we can assume that the install error
        // was due to lack of internet connection.
        // TODO(b/40927698): Consider setting the error status directly in
        // app.ts so that this can be reused by the voice menu when other
        // errors are added to the voice menu.
        if (!window.navigator.onLine) {
          return {isError: true, text: 'readingModeLanguageMenuNoInternet'};
        }
        // Show a generic error message.
        return {isError: true, text: 'languageMenuDownloadFailed'};
      case VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION:
        // If we get an allocation error but voices exist for the given
        // language, show an allocation error specific to downloading high
        // quality voices.
        if (this.availableVoices.some(
                voice => voice.lang.toLowerCase() === lang)) {
          return {isError: true, text: 'allocationErrorHighQuality'};
        }
        return {isError: true, text: 'allocationError'};
      case VoiceClientSideStatusCode.AVAILABLE:
      case VoiceClientSideStatusCode.NOT_INSTALLED:
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
