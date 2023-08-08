// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-system-preferences-page' is the settings page containing
 * system preferences settings.
 */

import '../os_settings_page/os_settings_animated_pages.js';
import '../settings_shared.css.js';
import '../os_reset_page/reset_card.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isPowerwashAllowed, isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {PrefsState} from '../common/types.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';

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
      },

      shouldShowResetCard_: {
        type: Boolean,
        value: () => {
          return isPowerwashAllowed();
        },
      },
    };
  }

  prefs: PrefsState;
  private section_: Section;
  private shouldShowResetCard_: boolean;

  override connectedCallback() {
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
