// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-live-translate' is a component for showing Live
 * Translate settings. It appears on the accessibility subpage
 * (chrome://settings/accessibility) on Mac and some versions of Windows and on
 * the captions subpage (chrome://settings/captions) on Linux, ChromeOS, and
 * other versions of Windows.
 */

import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '../controls/settings_dropdown_menu.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';

import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';

import type {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import type {LanguageHelper, LanguagesModel} from '../languages_page/languages_types.js';

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
        notify: true,
      },

      languageHelper: Object,

      enableLiveTranslateSubtitle_: {
        type: String,
        value: loadTimeData.getString('captionsEnableLiveTranslateSubtitle'),
      },

      languageOptions_: {
        type: Array,
        value: () => [],
      },

      translatableLanguages_: {
        type: Array,
        value: () => [],
      },
    };
  }

  languages: LanguagesModel;
  languageHelper: LanguageHelper;
  private enableLiveTranslateSubtitle_: string;
  private languageOptions_: DropdownMenuOptionList;
  private translatableLanguages_: DropdownMenuOptionList;

  override ready() {
    super.ready();
    this.languageHelper.whenReady().then(() => {
      this.translatableLanguages_ =
          this.languages?.supported
              .filter(language => {
                return this.languageHelper.isTranslateBaseLanguage(language);
              })
              .map(language => {
                return {value: language.code, name: language.displayName};
              }) as DropdownMenuOptionList;
    });
  }

  private onLiveTranslateEnabledChange_() {
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
