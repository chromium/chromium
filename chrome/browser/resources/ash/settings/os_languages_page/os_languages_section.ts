// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-languages-section' is the top-level settings section for
 * languages.
 */

import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './language_settings_card.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsState} from '../common/types.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';

import {LanguageHelper, LanguagesModel} from './languages_types.js';
import {getTemplate} from './os_languages_section.html.js';

const OsSettingsLanguagesSectionElementBase = I18nMixin(PolymerElement);

export class OsSettingsLanguagesSectionElement extends
    OsSettingsLanguagesSectionElementBase {
  static get is() {
    return 'os-settings-languages-section' as const;
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

      section_: {
        type: Number,
        value: Section.kLanguagesAndInput,
        readOnly: true,
      },

      /**
       * Set of languages from <settings-languages>
       */
      languages: Object,

      /**
       * Language helper API from <settings-languages>
       */
      languageHelper: Object,
    };
  }

  // Public API: Bidirectional data flow.
  /** Passed down to children. Do not access without using PrefsMixin. */
  prefs: PrefsState;

  languages: LanguagesModel|undefined;
  languageHelper: LanguageHelper|undefined;

  // Internal state.
  private section_: Section;
}

customElements.define(
    OsSettingsLanguagesSectionElement.is, OsSettingsLanguagesSectionElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsLanguagesSectionElement.is]: OsSettingsLanguagesSectionElement;
  }
}
