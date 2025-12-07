// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './category_setting_exceptions.js';
import './settings_category_default_radio_group.js';
import './site_settings_shared.css.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {ContentSettingsTypes} from './constants.js';
import type {SiteSettingsBrowserProxy} from './site_settings_browser_proxy.js';
import {SiteSettingsBrowserProxyImpl} from './site_settings_browser_proxy.js';
import {getTemplate} from './sound_page.html.js';

interface BlockAutoplayStatus {
  enabled: boolean;
  pref: chrome.settingsPrivate.PrefObject<boolean>;
}

const SoundPageElementBase =
    SettingsViewMixin(WebUiListenerMixin(PolymerElement));

export class SoundPageElement extends SoundPageElementBase {
  static get is() {
    return 'settings-sound-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      searchTerm: String,

      // Expose ContentSettingsTypes enum to the HTML template.
      contentSettingsTypesEnum_: {
        type: Object,
        value: ContentSettingsTypes,
      },

      blockAutoplayStatus_: {
        type: Object,
        value() {
          return {};
        },
      },

      enableBlockAutoplayContentSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableBlockAutoplayContentSetting');
        },
      },
    };
  }

  declare searchTerm: string;
  declare private blockAutoplayStatus_: BlockAutoplayStatus;
  declare private enableBlockAutoplayContentSetting_: boolean;

  private browserProxy_: SiteSettingsBrowserProxy =
      SiteSettingsBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.onBlockAutoplayStatusChanged_({
      pref: {
        key: '',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
      enabled: false,
    });

    this.addWebUiListener(
        'onBlockAutoplayStatusChanged',
        (status: BlockAutoplayStatus) =>
            this.onBlockAutoplayStatusChanged_(status));
  }


  // Called when the block autoplay status changes.
  private onBlockAutoplayStatusChanged_(autoplayStatus: BlockAutoplayStatus) {
    this.blockAutoplayStatus_ = autoplayStatus;
  }

  // Updates the block autoplay pref when the toggle is changed.
  private onBlockAutoplayToggleChange_(event: Event) {
    const target = event.target as SettingsToggleButtonElement;
    this.browserProxy_.setBlockAutoplayEnabled(target.checked);
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-sound-page': SoundPageElement;
  }
}

customElements.define(SoundPageElement.is, SoundPageElement);
