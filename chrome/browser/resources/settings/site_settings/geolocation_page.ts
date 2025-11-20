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

import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {ContentSetting, ContentSettingsTypes, SettingsState} from './constants.js';
import {getTemplate} from './geolocation_page.html.js';
import type {SiteSettingsBrowserProxy} from './site_settings_browser_proxy.js';
import {SiteSettingsBrowserProxyImpl} from './site_settings_browser_proxy.js';

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

      enablePermissionSiteSettingsRadioButton_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enablePermissionSiteSettingsRadioButton'),
      },

      /** Expose the Permissions SettingsState enum to HTML bindings. */
      settingsStateEnum_: {
        type: Object,
        value: SettingsState,
      },

      /** Expose ContentSettingsTypes enum to HTML bindings. */
      contentSettingsTypesEnum_: {
        type: Object,
        value: ContentSettingsTypes,
      },

      /** Expose ContentSetting enum to HTML bindings. */
      contentSettingEnum_: {
        type: Object,
        value: ContentSetting,
      },

      isLocationAllowed_: Boolean,
    };
  }

  declare searchTerm: string;
  declare private enablePermissionSiteSettingsRadioButton_: boolean;
  declare private isLocationAllowed_: boolean;
  private siteSettingsBrowserProxy_: SiteSettingsBrowserProxy =
      SiteSettingsBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();
    this.updateLocationState_();
  }

  private async updateLocationState_() {
    const [locationDefaultValue] = await Promise.all([
      this.siteSettingsBrowserProxy_.getDefaultValueForContentType(
          ContentSettingsTypes.GEOLOCATION),
    ]);
    this.isLocationAllowed_ =
        (locationDefaultValue.setting === ContentSetting.ASK);
  }

  private onLocationTopLevelRadioChanged_(event: CustomEvent<{value: string}>) {
    const radioButtonName = event.detail.value;
    switch (radioButtonName) {
      case 'location-block-radio-button':
        this.setPrefValue('generated.geolocation', SettingsState.BLOCK);
        this.isLocationAllowed_ = false;
        break;
      case 'location-ask-radio-button':
        this.setPrefValue('generated.geolocation', SettingsState.CPSS);
        this.isLocationAllowed_ = true;
        break;
    }
  }

  private onLocationTopLevelRadioChanged2_(
      event: CustomEvent<{value: boolean}>) {
    const selected = event.detail.value;
    if (selected) {
      this.setPrefValue('generated.geolocation', SettingsState.CPSS);
      this.isLocationAllowed_ = true;
    } else {
      this.setPrefValue('generated.geolocation', SettingsState.BLOCK);
      this.isLocationAllowed_ = false;
    }
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
