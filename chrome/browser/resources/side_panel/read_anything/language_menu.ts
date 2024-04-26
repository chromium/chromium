// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_hidden_style.css.js';
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
import {convertLangOrLocaleForVoicePackManager, VoicePackStatus} from './voice_language_util.js';

export interface LanguageMenuElement {
  $: {
    languageMenu: CrDialogElement,
  };
}

interface LanguageDropdownItem {
  language: string;
  checked: boolean;
  notificationText: string;
  ariaString: string;
  // A notification that's an "error" should be red and announce the error
  // properly for accessibility.
  isNotificationError: boolean;
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
      voicePackInstallStatus: {type: Object, notify: true},
    };
  }

  private languageSearchValue_: string;
  private voicePackInstallStatus: {[language: string]: VoicePackStatus} = {};

  private closeLanguageMenu_() {
    this.$.languageMenu.close();
  }

  private onClearSearchClick_() {
    this.languageSearchValue_ = '';
  }

  private onToggleChange_(event: DomRepeatEvent<LanguageDropdownItem>) {
    event.model.item.callback();
  }

  private computeAvailableLanguages_(
      availableVoices: SpeechSynthesisVoice[],
      localeToDisplayName: {[lang: string]: string},
      languageSearchValue: string|undefined,
      enabledLanguagesInPref: string[]): LanguageDropdownItem[] {
    if (!availableVoices) {
      return [];
    }

    const langsAndReadableLangs: Array<[string, string]> =
        [...new Set(availableVoices.map(({lang}) => lang))].map(
            lang => ([
              lang,
              (localeToDisplayName && lang in localeToDisplayName) ?
                  localeToDisplayName[lang] :
                  lang,
            ]));

    return langsAndReadableLangs
        .filter(([_, readableLang]) => {
          if (languageSearchValue) {
            return readableLang.toLowerCase().includes(
                languageSearchValue.toLowerCase());
          } else {
            return true;
          }
        })
        .map(([lang, readableLang]) => ({
               language: readableLang,
               checked: enabledLanguagesInPref &&
                   enabledLanguagesInPref.includes(lang),
               notificationText:
                   this.getNotificationText(lang, this.voicePackInstallStatus),
               isNotificationError:
                   this.isNotificationError(lang, this.voicePackInstallStatus),
               ariaString:
                   this.getAriaSetting(lang, this.voicePackInstallStatus),
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

  private getAriaSetting(
      lang: string,
      voicePackInstallStatus: {[language: string]: VoicePackStatus}): string {
    return this.isNotificationError(lang, voicePackInstallStatus) ? `polite` :
                                                                    `off`;
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
        return `${this.i18n('readingModeLanguageMenuDownloading')}`;
      case VoicePackStatus.INSTALL_ERROR:
        // There's not a specific error code from the language pack installer
        // for internet connectivity, but if there's an installation error
        // and we detect we're offline, we can assume that the install error
        // was due to lack of internet connection.
        // TODO(b/40927698): Consider setting the error status directly in
        // app.ts so that this can be reused by the voice menu when other
        // errors are added to the voice menu.
        if (!window.navigator.onLine) {
          return `${this.i18n('readingModeLanguageMenuNoInternet')}`;
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

  showDialog() {
    this.$.languageMenu.showModal();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'language-menu': LanguageMenuElement;
  }
}

customElements.define(LanguageMenuElement.is, LanguageMenuElement);
