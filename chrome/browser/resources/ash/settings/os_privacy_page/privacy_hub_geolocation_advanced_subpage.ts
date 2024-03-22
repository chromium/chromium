// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-hub-geolocation-advanced-subpage' contains advance
 * settings regarding geolocation. For example: Google Location Accuracy.
 *
 */

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isSecondaryUser} from '../common/load_time_booleans.js';

import {getTemplate} from './privacy_hub_geolocation_advanced_subpage.html.js';

const SettingsPrivacyHubGeolocationAdvancedSubpageBase =
    PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)));

export class SettingsPrivacyHubGeolocationAdvancedSubpage extends
    SettingsPrivacyHubGeolocationAdvancedSubpageBase {
  static get is() {
    return 'settings-privacy-hub-geolocation-advanced-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether the location access control should be displayed in Privacy Hub.
       */
      showPrivacyHubLocationControl_: {
        type: Boolean,
        readOnly: true,
        value: function() {
          return loadTimeData.getBoolean('showPrivacyHubLocationControl');
        },
      },
      isSecondaryUser_: {
        type: Boolean,
        value() {
          return isSecondaryUser();
        },
        readOnly: true,
      },
    };
  }

  private settingControlledByPrimaryUserText_(): string {
    return this.i18n(
        'geolocationControlledByPrimaryUserText',
        loadTimeData.getString('primaryUserEmail'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubGeolocationAdvancedSubpage.is]:
    SettingsPrivacyHubGeolocationAdvancedSubpage;
  }
}

customElements.define(
  SettingsPrivacyHubGeolocationAdvancedSubpage.is,
  SettingsPrivacyHubGeolocationAdvancedSubpage);
