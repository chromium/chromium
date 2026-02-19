// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './category_setting_exceptions.js';
import './settings_category_default_radio_group.js';
import './site_settings_shared.css.js';
import '../controls/collapse_radio_button.js';
import '../controls/settings_radio_group.js';
import '../privacy_icons.html.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {ContentSetting, ContentSettingsTypes, SettingsState} from './constants.js';
import {getTemplate} from './geolocation_page.html.js';

const GeolocationPageElementBase =
    SettingsViewMixin(PrefsMixin(PolymerElement));

export class GeolocationPageElement extends GeolocationPageElementBase {
  static get is() {
    return 'settings-geolocation-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      searchTerm: {
        type: String,
        notify: true,
        value: '',
      },

      /** Expose the Permissions SettingsState enum to HTML bindings. */
      settingsStateEnum_: {
        type: Object,
        value: SettingsState,
      },

      /** Expose ContentSettingTypes enum to HTML bindings. */
      contentSettingsTypesEnum_: {
        type: Object,
        value: ContentSettingsTypes,
      },

      /**
       * Glue property to connect the category default setting value to the
       * visibility of the additional CPSS options.
       */
      locationSettingValue_: String,
    };
  }

  declare searchTerm: string;
  declare private locationSettingValue_: string;

  private isEqualToAsk_(locationSettingValue: string): boolean {
    return locationSettingValue === ContentSetting.ASK;
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-geolocation-page': GeolocationPageElement;
  }
}

customElements.define(GeolocationPageElement.is, GeolocationPageElement);
