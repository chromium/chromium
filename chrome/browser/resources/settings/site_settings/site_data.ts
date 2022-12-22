// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-site-data' is the polymer element for showing the
 * settings for site data under Site Settings.
 */
import '../controls/settings_radio_group.js';
import '../prefs/prefs.js';
import '../privacy_page/collapse_radio_button.js';
import './site_list.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';
import {SettingsCollapseRadioButtonElement} from '../privacy_page/collapse_radio_button.js';

import {ContentSetting, ContentSettingsTypes} from './constants.js';
import {getTemplate} from './site_data.html.js';

export interface SettingsSiteDataElement {
  $: {
    defaultGroup: SettingsRadioGroupElement,
    defaultAllow: SettingsCollapseRadioButtonElement,
    defaultSessionOnly: SettingsCollapseRadioButtonElement,
    defaultBlock: SettingsCollapseRadioButtonElement,
  };
}

const SettingsSiteDataElementBase = PrefsMixin(PolymerElement);

export class SettingsSiteDataElement extends SettingsSiteDataElementBase {
  static get is() {
    return 'settings-site-data';
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

      /** Current search term. */
      searchTerm: {
        type: String,
        notify: true,
        value: '',
      },

      cookiesContentSettingType_: {
        type: String,
        value: ContentSettingsTypes.COOKIES,
      },

      /** Expose ContentSetting enum to HTML bindings. */
      contentSettingEnum_: {
        type: Object,
        value: ContentSetting,
      },

      exceptionListsReadOnly_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [`onGeneratedPrefsUpdated_(
        prefs.generated.cookie_default_content_setting)`];
  }

  searchTerm: string;
  private cookiesContentSettingType_: ContentSettingsTypes;
  private exceptionListsReadOnly_: boolean;

  private onGeneratedPrefsUpdated_() {
    const pref = this.getPref('generated.cookie_default_content_setting');

    // If the pref is managed this implies a content setting policy is present
    // and the exception lists should be disabled.
    this.exceptionListsReadOnly_ =
        pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-site-data': SettingsSiteDataElement;
  }
}

customElements.define(SettingsSiteDataElement.is, SettingsSiteDataElement);
