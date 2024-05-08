// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-live-translate' is a component for showing Live
 * Translate settings. It appears on the Audio and captions subpage
 * (chrome://os-settings/audioAndCaptions) on ChromeOS.
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import '../controls/settings_dropdown_menu.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import type {LanguageHelper, LanguagesModel} from '../os_languages_page/languages_types.js';

import {getTemplate} from './live_translate_section.html.js';

const SettingsLiveTranslateElementBase =
    WebUiListenerMixin(PrefsMixin(PolymerElement));

export interface SettingsLiveTranslateElement {
  $: {
    liveTranslateToggleButton: SettingsToggleButtonElement,
  };
}

export class SettingsLiveTranslateElement extends
    SettingsLiveTranslateElementBase {
  static get is() {
    return 'settings-live-translate';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
      },

      languageHelper: Object,

      translatableLanguages_: {
        type: Array,
        value: () => [],
      },
    };
  }

  languages: LanguagesModel;
  languageHelper: LanguageHelper;
  private translatableLanguages_: DropdownMenuOptionList;

  override ready(): void {
    super.ready();
    this.languageHelper.whenReady().then(() => {
      this.translatableLanguages_ =
          this.languages.supported
              .filter((language: chrome.languageSettingsPrivate.Language) => {
                return this.languageHelper.isLanguageTranslatable(language);
              })
              .map((language: chrome.languageSettingsPrivate.Language) => {
                return {value: language.code, name: language.displayName};
              }) as DropdownMenuOptionList;
    });
  }

  private onLiveTranslateEnabledChange_(): void {
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.LiveTranslate.EnableFromSettings',
        this.$.liveTranslateToggleButton.checked);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-live-translate': SettingsLiveTranslateElement;
  }
}

customElements.define(
    SettingsLiveTranslateElement.is, SettingsLiveTranslateElement);
