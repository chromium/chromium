// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-hub-geolocation-subpage' contains a detailed overview about
 * the state of the system geolocation access.
 */

import {DropdownMenuOptionList, SettingsDropdownMenuElement} from '/shared/settings/controls/settings_dropdown_menu.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_hub_geolocation_subpage.html.js';
import {LOCATION_PERMISSION_CHANGE_FROM_SETTINGS_HISTOGRAM_NAME} from './privacy_hub_metrics_util.js';

/**
 * Geolocation access levels for the ChromeOS system.
 * This must be kept in sync with `GeolocationAccessLevel` in
 * ash/constants/geolocation_access_level.h
 */
export enum GeolocationAccessLevel {
  DISALLOWED = 0,
  ALLOWED = 1,
  ONLY_ALLOWED_FOR_SYSTEM = 2,
}

export const GEOLOCATION_ACCESS_LEVEL_ENUM_SIZE =
    Object.keys(GeolocationAccessLevel).length;

export interface SettingsPrivacyHubGeolocationSubpage {
  $: {
    geolocationDropdown: SettingsDropdownMenuElement,
  };
}

const SettingsPrivacyHubGeolocationSubpageBase =
    PrefsMixin(I18nMixin(PolymerElement));

export class SettingsPrivacyHubGeolocationSubpage extends
    SettingsPrivacyHubGeolocationSubpageBase {
  static get is() {
    return 'settings-privacy-hub-geolocation-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      geolocationMapTargets_: {
        type: Object,
        value(this: SettingsPrivacyHubGeolocationSubpage) {
          return [
            {
              value: GeolocationAccessLevel.ALLOWED,
              name: this.i18n('geolocationAccessLevelAllowed'),
            },
            {
              value: GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM,
              name: this.i18n('geolocationAccessLevelOnlyAllowedForSystem'),
            },
            {
              value: GeolocationAccessLevel.DISALLOWED,
              name: this.i18n('geolocationAccessLevelDisallowed'),
            },
          ];
        },
      },
    };
  }

  private geolocationMapTargets_: DropdownMenuOptionList;

  private recordMetric_(): void {
    const accessLevel = this.$.geolocationDropdown.pref!.value;

    chrome.metricsPrivate.recordEnumerationValue(
        LOCATION_PERMISSION_CHANGE_FROM_SETTINGS_HISTOGRAM_NAME, accessLevel,
        GEOLOCATION_ACCESS_LEVEL_ENUM_SIZE);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubGeolocationSubpage.is]:
        SettingsPrivacyHubGeolocationSubpage;
  }
}

customElements.define(
    SettingsPrivacyHubGeolocationSubpage.is,
    SettingsPrivacyHubGeolocationSubpage);
