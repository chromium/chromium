// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/icons_lit.html.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import './icons.html.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {toastDurationMs, ToolbarEvent} from './common.js';
import {getTemplate} from './language_menu.html.js';
import {AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleForVoicePackManager, VoiceClientSideStatusCode} from './voice_language_util.js';

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

const LanguageMenuElementBase = WebUiListenerMixin(I18nMixin(PolymerElement));

export class LanguageMenuElement extends LanguageMenuElementBase {
  static get is() {
    return 'language-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      enabledLangs: Array,
      availableVoices: Array,
      languageSearchValue_: String,
      localeToDisplayName: Object,
      voicePackInstallStatus: {
        type: Object,
        observer: 'voicePackInstallStatusChanged_',
      },
      selectedLang: String,
      currentNotifications_: Array,
      lastDownloadedLang: String,
      toastTitle_: {
        type: String,
        computed: 'getLanguageDownloadedTitle_(localeToDisplayName,' +
            'lastDownloadedLang)',
      },
      availableLanguages_: {
        type: Array,
        computed:
            'computeAvailableLanguages_(availableVoices,localeToDisplayName,' +
            'currentNotifications_,selectedLang,languageSearchValue_,' +
            'enabledLangs)',
      },
    };
  }

  selectedLang: string;
  localeToDisplayName: {[lang: string]: string} = {};
  enabledLangs: string[];
  lastDownloadedLang: string;

  availableVoices: SpeechSynthesisVoice[];
  private languageSearchValue_: string;
  private toastDuration_: number = toastDurationMs;
  voicePackInstallStatus: {[language: string]: VoiceClientSideStatusCode};
  private readonly availableLanguages_: LanguageDropdownItem[];
  // Use this variable instead of AVAILABLE_GOOGLE_TTS_LOCALES
  // directly to better aid in testing.
  baseLanguages: Set<string> = AVAILABLE_GOOGLE_TTS_LOCALES;

  // A non-Google language is one that's not associated with a Google voice
  // that can be downloaded from the language pack.
  private nonGoogleLanguages: string[] = [];

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
  private voicePackInstallStatusChanged_(
      newValue: {[language: string]: VoiceClientSideStatusCode},
      oldValue?: {[language: string]: VoiceClientSideStatusCode}) {
    for (const key of Object.keys(newValue)) {
      const newStatus = newValue[key];
      // Since the downloading messages are cleared quickly, we should still
      // show "downloading" notifications, even if they were previously shown.
      if (isDownloading(newStatus)) {
        this.setNotification(key, newStatus);
      } else if (oldValue && oldValue[key] !== newStatus) {
        // Update the notification status for recently changed language keys.
        // Only show updates that occur while the language menu is open- don't
        // show notifications if updates occurred before the menu opened.
        this.setNotification(key, newStatus);
      }
    }
  }

  private setNotification(lang: string, status: VoiceClientSideStatusCode) {
    this.currentNotifications_ = {
      ...this.currentNotifications_,
      [lang]: status,
    };
  }
  private closeLanguageMenu_() {
    this.$.languageMenu.close();
  }

  private onClearSearchClick_() {
    this.languageSearchValue_ = '';
  }

  private onToggleChange_(event: DomRepeatEvent<LanguageDropdownItem>) {
    event.model.item.callback();
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

  // TODO(b/40927698): Investigate removing currentNotifications as a
  // dependency.
  private computeAvailableLanguages_(): LanguageDropdownItem[] {
    if (!this.availableVoices) {
      return [];
    }

    // Ensure this is cleared each time we recompute available languages.
    this.nonGoogleLanguages = [];

    const selectedLangLowerCase = this.selectedLang?.toLowerCase();
    // Ensure we've added the available pack manager supported languages to
    // the language menu first, only on ChromeOS.
    const langsAndReadableLangs: Array<[string, string]> =
        chrome.readingMode.isLanguagePackDownloadingEnabled &&
            chrome.readingMode.isChromeOsAsh ?
        Array.from(
            this.baseLanguages, (key) => [key, this.getDisplayName(key)]) :
        [];

    // Next, add any other supported languages to the menu, if they don't
    // already exist.
    this.availableVoices.forEach((voice) => {
      const lang = voice.lang;
      if (!langsAndReadableLangs.some(
              ([key, _]) => key === lang.toLowerCase())) {
        langsAndReadableLangs.push([
          lang.toLowerCase(),
          this.getDisplayName(lang),
        ]);

        if (chrome.readingMode.isLanguagePackDownloadingEnabled) {
          this.nonGoogleLanguages.push(lang.toLowerCase());
        }
      }
    });

    // Sort the list of languages alphabetically by display name.
    langsAndReadableLangs.sort(([, firstDisplay], [, secondDisplay]) => {
      return firstDisplay.localeCompare(secondDisplay);
    });

    return langsAndReadableLangs
        .filter(([lang, readableLang]) => {
          if (this.languageSearchValue_) {
            // In addition to matching the readable language, also allow
            // the search term to extend to the language code to make
            // searching for specific languages easier. e.g. 'pt-br' will
            // match with Portugues (Brasil)
            return readableLang.toLowerCase().includes(
                       this.languageSearchValue_.toLowerCase()) ||
                lang.includes(this.languageSearchValue_.toLowerCase());
          } else {
            return true;
          }
        })
        .map(
            ([lang, readableLang]) => ({
              readableLanguage: readableLang,
              checked: this.enabledLangs && this.enabledLangs.includes(lang),
              languageCode: lang,
              notification: {
                isError: this.isNotificationError(lang),
                text: this.getNotificationText(lang),
              },
              disabled: this.enabledLangs && this.enabledLangs.includes(lang) &&
                  (lang.toLowerCase() === selectedLangLowerCase),
              callback: () => this.dispatchEvent(
                  new CustomEvent(ToolbarEvent.LANGUAGE_TOGGLE, {
                    bubbles: true,
                    composed: true,
                    detail: {
                      language: lang,
                    },
                  })),
            }));
  }

  private isNotificationError(lang: string): boolean {
    // Don't show notification text for a non-Google TTS language, as we're
    // not attempting a download.
    if (this.nonGoogleLanguages.includes(lang)) {
      return false;
    }

    const voicePackLanguage = convertLangOrLocaleForVoicePackManager(lang);

    if (!voicePackLanguage) {
      // If the voice pack language doesn't exist, no need to update the
      // notification error status.
      return false;
    }

    const notification: VoiceClientSideStatusCode|undefined =
        this.currentNotifications_[voicePackLanguage];

    if (notification === undefined) {
      return false;
    }

    // TODO(b/40927698): In the future, some of our install error messages
    // might not be set to an "error" in the notification status span, so
    // be more specific.
    return notification === VoiceClientSideStatusCode.ERROR_INSTALLING ||
        notification === VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION;
  }

  private getNotificationText(lang: string): string {
    // Don't show notification text for a non-Google TTS language, as we're
    // not attempting a download.
    if (this.nonGoogleLanguages.includes(lang)) {
      return '';
    }

    // Make sure to convert the lang string, otherwise there could be a
    // mismatch in a language and locale and what is stored in the installation
    // map.
    const voicePackLanguage = convertLangOrLocaleForVoicePackManager(lang);

    // No need to check the install status if the language is missing.
    if (!voicePackLanguage) {
      return '';
    }
    const notification: VoiceClientSideStatusCode|undefined =
        this.currentNotifications_[voicePackLanguage];

    if (notification === undefined) {
      return '';
    }

    // TODO(b/300259625): Show more error messages.
    switch (notification) {
      case VoiceClientSideStatusCode.SENT_INSTALL_REQUEST:
      case VoiceClientSideStatusCode.SENT_INSTALL_REQUEST_ERROR_RETRY:
      case VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE:
        return 'readingModeLanguageMenuDownloading';
      case VoiceClientSideStatusCode.ERROR_INSTALLING:
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
        // Show a generic error message.
        return 'languageMenuDownloadFailed';
      case VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION:
        // If we get an allocation error but voices exist for the given
        // language, show an allocation error specific to downloading high
        // quality voices.
        if (this.availableVoices.some(
                voice => voice.lang.toLowerCase() === lang)) {
          return 'allocationErrorHighQuality';
        }
        return 'allocationError';
      case VoiceClientSideStatusCode.AVAILABLE:
      case VoiceClientSideStatusCode.NOT_INSTALLED:
        return '';
      default:
        // This ensures the switch statement is exhaustive
        return notification satisfies never;
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


  private searchHasLanguages(): boolean {
    // We should only show the "No results" string when there are no available
    // languages and there is a valid search term.
    return (this.availableLanguages_.length > 0) ||
        (!this.languageSearchValue_) ||
        (this.languageSearchValue_.trim().length === 0);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'language-menu': LanguageMenuElement;
  }
}

customElements.define(LanguageMenuElement.is, LanguageMenuElement);
