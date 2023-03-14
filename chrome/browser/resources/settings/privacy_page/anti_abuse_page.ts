// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-anti-abuse-page' is the settings page containing anti-abuse
 * settings.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {ContentSetting, ContentSettingsTypes} from '../site_settings/constants.js';
import {SiteSettingsMixin} from '../site_settings/site_settings_mixin.js';

import {getTemplate} from './anti_abuse_page.html.js';

const AntiAbuseElementBase = SiteSettingsMixin(PolymerElement);

export class SettingsAntiAbusePageElement extends AntiAbuseElementBase {
  static get is() {
    return 'settings-anti-abuse-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preference object used to keep track of the selected content setting
       * option.
       */
      pref_: {
        type: Object,
        value() {
          return {type: chrome.settingsPrivate.PrefType.BOOLEAN};
        },
      },
    };
  }

  private pref_: chrome.settingsPrivate.PrefObject<boolean>;

  override ready() {
    super.ready();

    this.initializeToggleValue_();
  }

  private async initializeToggleValue_() {
    const defaultValue = await this.browserProxy.getDefaultValueForContentType(
        ContentSettingsTypes.ANTI_ABUSE);
    this.set('pref_.value', this.computeIsSettingEnabled(defaultValue.setting));
  }

  /**
   * A handler for changing the default permission value for a the anti-abuse
   * content type.
   */
  private onToggleChange_(e: Event) {
    const target = e.target as SettingsToggleButtonElement;
    this.browserProxy.setDefaultValueForContentType(
        ContentSettingsTypes.ANTI_ABUSE,
        target.checked ? ContentSetting.ALLOW : ContentSetting.BLOCK);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-anti-abuse-page': SettingsAntiAbusePageElement;
  }
}

customElements.define(
    SettingsAntiAbusePageElement.is, SettingsAntiAbusePageElement);
