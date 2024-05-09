// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import './icons.html.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import type {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './language_menu.html.js';
import {AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleForVoicePackManager, VoicePackStatus} from './voice_language_util.js';

export interface LanguageMenuElement {
  $: {
    languageMenu: CrDialogElement,
  };
}

interface Notification {
  isError: boolean;
  text: string|undefined;
}

interface LanguageDropdownItem {
  readableLanguage: string;
  checked: boolean;
  languageCode: string;
  notification: Notification;
  // Whether this toggle should be disabled
  disabled: boolean;
  callback: () => void;
}

const LanguageMenuElementBase = WebUiListenerMixin(I18nMixin(PolymerElement));

export const LANGUAGE_TOGGLE_EVENT = 'voice-language-toggle';

export class LanguageMenuElement extends LanguageMenuElementBase {
  static get is() {
    return 'language-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      enabledLanguagesInPref: Array,
      availableVoices: Array,
      languageSearchValue_: String,
      localeToDisplayName: Object,
      voicePackInstallStatus: Object,
      selectedLang: String,
      availableLanguages_: {
        type: Array,
        computed:
            'computeAvailableLanguages_(availableVoices,localeToDisplayName,' +
            'voicePackInstallStatus,selectedLang,languageSearchValue_)',
      },
    };
  }

  private languageSearchValue_: string;
  private readonly voicePackInstallStatus:
      {[language: string]: VoicePackStatus};
  private readonly enabledLanguagesInPref: string[];
  private readonly availableLanguages_: LanguageDropdownItem[];
  // Use this variable instead of AVAILABLE_GOOGLE_TTS_LOCALES
  // directly to better aid in testing.
  private baseLanguages = AVAILABLE_GOOGLE_TTS_LOCALES;

  private closeLanguageMenu_() {
    this.$.languageMenu.close();
  }

  private onCloseDialog_() {
    this.onClearSearchClick_();
  }

  private onClearSearchClick_() {
    this.languageSearchValue_ = '';
  }

  private onToggleChange_(event: DomRepeatEvent<LanguageDropdownItem>) {
    event.model.item.callback();
  }

  private getDisplayName(
      localeToDisplayName: {[lang: string]: string}, lang: string) {
    return (localeToDisplayName && lang in localeToDisplayName) ?
        localeToDisplayName[lang] :
        lang;
  }

  private computeAvailableLanguages_(
      availableVoices: SpeechSynthesisVoice[],
      localeToDisplayName: {[lang: string]: string},
      voicePackInstallStatus: {[language: string]: VoicePackStatus},
      selectedLang: string|undefined,
      languageSearchValue: string|undefined): LanguageDropdownItem[] {
    if (!availableVoices) {
      return [];
    }

    const selectedLangLowerCase = selectedLang?.toLowerCase();
    // Ensure we've added the available pack manager supported languages to
    // the language menu first, only on ChromeOS.
    const langsAndReadableLangs: Array<[string, string]> =
        chrome.readingMode.isLanguagePackDownloadingEnabled &&
            chrome.readingMode.isChromeOsAsh ?
        Array.from(
            this.baseLanguages,
            (key) => [key, this.getDisplayName(localeToDisplayName, key)]) :
        [];

    // Next, add any other supported languages to the menu, if they don't
    // already exist.
    availableVoices.forEach((voice) => {
      const lang = voice.lang;
      if (!langsAndReadableLangs.some(
              ([key, _]) => key === lang.toLowerCase())) {
        langsAndReadableLangs.push([
          lang.toLowerCase(),
          this.getDisplayName(localeToDisplayName, lang),
        ]);
      }
    });

    // Sort the list of languages alphabetically by display name.
    langsAndReadableLangs.sort(([, firstDisplay], [, secondDisplay]) => {
      return firstDisplay.localeCompare(secondDisplay);
    });

    return langsAndReadableLangs
        .filter(([_, readableLang]) => {
          if (languageSearchValue) {
            return readableLang.toLowerCase().includes(
                languageSearchValue.toLowerCase());
          } else {
            return true;
          }
        })
        .map(
            ([lang, readableLang]) => ({
              readableLanguage: readableLang,
              checked: this.enabledLanguagesInPref &&
                  this.enabledLanguagesInPref.includes(lang),
              languageCode: lang,
              notification: {
                isError: this.isNotificationError(lang, voicePackInstallStatus),
                text: this.getNotificationText(lang, voicePackInstallStatus),
              },
              disabled: this.enabledLanguagesInPref &&
                  this.enabledLanguagesInPref.includes(lang) &&
                  (lang.toLowerCase() === selectedLangLowerCase),
              callback: () =>
                  this.dispatchEvent(new CustomEvent(LANGUAGE_TOGGLE_EVENT, {
                    bubbles: true,
                    composed: true,
                    detail: {
                      language: lang,
                    },
                  })),
            }));
  }

  private computeAriaSetting(isError: boolean): string {
    return isError ? `polite` : `off`;
  }

  private isNotificationError(
      lang: string,
      voicePackInstallStatus: {[language: string]: VoicePackStatus}): boolean {
    const voicePackLanguage = convertLangOrLocaleForVoicePackManager(lang);

    if (!voicePackLanguage) {
      // If the voice pack language doesn't exist, no need to update the
      // notification error status.
      return false;
    }

    const notification: VoicePackStatus|undefined =
        voicePackInstallStatus[voicePackLanguage];

    if (notification === undefined) {
      return false;
    }

    // TODO(b/40927698): In the future, some of our install error messages
    // might not be set to an "error" in the notification status span, so
    // be more specific.
    return notification === VoicePackStatus.INSTALL_ERROR;
  }

  private getNotificationText(
      lang: string,
      voicePackInstallStatus: {[language: string]: VoicePackStatus}): string {
    // Make sure to convert the lang string, otherwise there could be a
    // mismatch in a language and locale and what is stored in the installation
    // map.
    const voicePackLanguage = convertLangOrLocaleForVoicePackManager(lang);

    // No need to check the install status if the language is missing.
    if (!voicePackLanguage) {
      return '';
    }
    const notification: VoicePackStatus|undefined =
        voicePackInstallStatus[voicePackLanguage];

    if (notification === undefined) {
      return '';
    }

    // TODO(b/300259625): Show more error messages.
    switch (notification) {
      case VoicePackStatus.INSTALLING:
      case VoicePackStatus.DOWNLOADED:
        return 'readingModeLanguageMenuDownloading';
      case VoicePackStatus.INSTALL_ERROR:
        // There's not a specific error code from the language pack installer
        // for internet connectivity, but if there's an installation error
        // and we detect we're offline, we can assume that the install error
        // was due to lack of internet connection.
        // TODO(b/40927698): Consider setting the error status directly in
        // app.ts so that this can be reused by the voice menu when other
        // errors are added to the voice menu.
        if (!window.navigator.onLine) {
          return 'readingModeLanguageMenuNoInternet';
        }
        return '';
      case VoicePackStatus.NONE:
      case VoicePackStatus.EXISTS:
      case VoicePackStatus.INSTALLED:
      case VoicePackStatus.REMOVED_BY_USER:
      default:
        return '';
    }
  }

  // Runtime errors were thrown when this.i18n() was called in a Polymer
  // computed bindining callback function, so instead we call this.i18n from the
  // html via a wrapper.
  private i18nWraper(s: string): string {
    if (!s) {
      return '';
    }
    return `${this.i18n(s)}`;
  }

  showDialog() {
    this.$.languageMenu.showModal();
  }

  private searchHasLanguages(
      availableLanguages: LanguageDropdownItem[],
      languageSearchValue: string): boolean {
    // We should only show the "No results" string when there are no available
    // languages and there is a valid search term.
    return (availableLanguages.length > 0) || (!languageSearchValue) ||
        (languageSearchValue.trim().length === 0);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'language-menu': LanguageMenuElement;
  }
}

customElements.define(LanguageMenuElement.is, LanguageMenuElement);
