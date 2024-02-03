// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-system-preferences-page' is the settings page containing
 * system preferences settings.
 */

import '../date_time_page/date_time_settings_card.js';
import '../os_files_page/files_settings_card.js';
import '../os_languages_page/language_settings_card.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../os_reset_page/reset_settings_card.js';
import '../os_search_page/search_and_assistant_settings_card.js';
import '../settings_shared.css.js';
import './multitasking_settings_card.js';
import './startup_settings_card.js';
import './storage_and_power_settings_card.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isAssistantAllowed, isExternalStorageEnabled, isGuest, isPowerwashAllowed, isQuickAnswersSupported, isRevampWayfindingEnabled, shouldShowMultitasking, shouldShowStartup} from '../common/load_time_booleans.js';
import {PrefsState} from '../common/types.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {LanguageHelper, LanguagesModel} from '../os_languages_page/languages_types.js';
import {routes} from '../router.js';

import {getTemplate} from './system_preferences_page.html.js';

const SettingsSystemPreferencesPageElementBase = I18nMixin(PolymerElement);

export class SettingsSystemPreferencesPageElement extends
    SettingsSystemPreferencesPageElementBase {
  static get is() {
    return 'settings-system-preferences-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      section_: {
        type: Number,
        value: Section.kSystemPreferences,
        readOnly: true,
      },

      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Set of languages from <settings-languages>
       */
      languages: Object,

      /**
       * Language helper API from <settings-languages>
       */
      languageHelper: Object,

      /**
       * This is used to cache the current time zone display name selected from
       * <timezone-selector> via bi-directional data-binding.
       */
      activeTimeZoneDisplayName_: {
        type: String,
        value: loadTimeData.getString('timeZoneName'),
      },

      shouldShowFilesSettingsCard_: {
        type: Boolean,
        value: () => {
          return !isGuest();
        },
      },

      shouldShowMultitaskingCard_: {
        type: Boolean,
        value: () => {
          return shouldShowMultitasking();
        },
      },

      shouldShowResetSettingsCard_: {
        type: Boolean,
        value: () => {
          return isPowerwashAllowed();
        },
      },

      isQuickAnswersSupported_: {
        type: Boolean,
        value: () => {
          return isQuickAnswersSupported();
        },
      },

      isAssistantAllowed_: {
        type: Boolean,
        value: () => {
          return isAssistantAllowed();
        },
      },

      isExternalStorageEnabled_: {
        type: Boolean,
        value: () => {
          return isExternalStorageEnabled();
        },
      },

      shouldStampGoogleDriveSubpage_: {
        type: Boolean,
        value: () => {
          return !!routes.GOOGLE_DRIVE;
        },
      },

      shouldStampOfficeSubpage_: {
        type: Boolean,
        value: () => {
          return !!routes.OFFICE;
        },
      },

      /**
       * Determines if the startup settings card is visible.
       */
      shouldShowStartupSettingsCard_: {
        type: Boolean,
        value: () => {
          return shouldShowStartup();
        },
        readOnly: true,
      },
    };
  }

  prefs: PrefsState|undefined;

  // Languages subsection
  languages: LanguagesModel|undefined;
  languageHelper: LanguageHelper|undefined;

  private section_: Section;

  // Date and Time subsection
  private activeTimeZoneDisplayName_: string;

  // Files subsection
  private shouldShowFilesSettingsCard_: boolean;
  private shouldStampGoogleDriveSubpage_: boolean;
  private shouldStampOfficeSubpage_: boolean;

  // Multitasking subsection
  private shouldShowMultitaskingCard_: boolean;

  // Reset subsection
  private shouldShowResetSettingsCard_: boolean;

  // Search and Assistant subsection
  private isQuickAnswersSupported_: boolean;
  private isAssistantAllowed_: boolean;

  // Startup subsection
  private readonly shouldShowStartupSettingsCard_: boolean;

  // Storage and Power subsection
  private isExternalStorageEnabled_: boolean;

  override connectedCallback(): void {
    super.connectedCallback();

    assert(
        isRevampWayfindingEnabled(),
        'OsSettingsRevampWayfinding feature must be enabled.');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsSystemPreferencesPageElement.is]:
        SettingsSystemPreferencesPageElement;
  }
}

customElements.define(
    SettingsSystemPreferencesPageElement.is,
    SettingsSystemPreferencesPageElement);
