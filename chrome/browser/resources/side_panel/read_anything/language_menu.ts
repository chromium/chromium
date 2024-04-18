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
      availableLanguages_: {
        type: Array,
        computed: 'computeAvailableLanguages_(availableVoices,' +
            ' localeToDisplayName, enabledLanguagesInPref)',
      },
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
      enabledLanguagesInPref: string[]): LanguageDropdownItem[] {
    const allLanguages = availableVoices.map(
        voice => (localeToDisplayName && voice.lang in localeToDisplayName) ?
            localeToDisplayName[voice.lang] :
            voice.lang);
    return [...new Set(allLanguages)].map(
        lang => ({
          language: lang,
          checked: enabledLanguagesInPref
            && enabledLanguagesInPref.includes(lang),
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

  private filterSearchLanguages_(
      availableLanguages: LanguageDropdownItem[],
      languageSearchValue: string): LanguageDropdownItem[] {
    if (!languageSearchValue) {
      return availableLanguages;
    } else {
      const languageSearchValueLowerCase = languageSearchValue.toLowerCase();
      return availableLanguages.filter(
          languageDropDownItem =>
              languageDropDownItem.language.toLowerCase().includes(
                  languageSearchValueLowerCase));
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
