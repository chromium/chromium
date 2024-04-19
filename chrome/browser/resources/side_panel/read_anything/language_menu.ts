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
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import type {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './language_menu.html.js';

export interface LanguageMenuElement {
  $: {
    languageMenu: CrDialogElement,
  };
}

interface LanguageDropdownItem {
  language: string;
  checked: boolean;
  callback: () => void;
}

const LanguageMenuElementBase = WebUiListenerMixin(PolymerElement);

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
    };
  }

  private languageSearchValue_: string;

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
