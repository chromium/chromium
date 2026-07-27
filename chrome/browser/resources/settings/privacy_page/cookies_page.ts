// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-cookies-page' is the settings page containing cookies
 * settings.
 */

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/collapse_radio_button.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../privacy_icons.html.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import '../site_settings/site_list.js';
import './do_not_track_toggle.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';
import {ContentSetting, ContentSettingsTypes} from '../site_settings/constants.js';
import {ThirdPartyCookieBlockingSetting} from '../site_settings/site_settings_browser_proxy.js';

import {getTemplate} from './cookies_page.html.js';

const SettingsCookiesPageElementBase = SettingsViewMixin(
    WebUiListenerMixin(I18nMixin(PrefsMixin(PolymerElement))));

export class SettingsCookiesPageElement extends SettingsCookiesPageElementBase {
  static get is() {
    return 'settings-cookies-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Current search term.
       */
      searchTerm: {
        type: String,
        notify: true,
        value: '',
      },

      thirdPartyCookieBlockingSettingEnum_: {
        type: Object,
        value: ThirdPartyCookieBlockingSetting,
      },

      contentSettingEnum_: {
        type: Object,
        value: ContentSetting,
      },

      cookiesContentSettingType_: {
        type: String,
        value: ContentSettingsTypes.COOKIES,
      },

      isRelatedWebsiteSetsUiEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isRelatedWebsiteSetsUiEnabled'),
      },
    };
  }

  declare searchTerm: string;
  declare private cookiesContentSettingType_: ContentSettingsTypes;
  declare private isRelatedWebsiteSetsUiEnabled_: boolean;

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  private onSiteDataClick_() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_ALL);
  }

  private onThirdPartyCookieBlockingSettingChanged_() {
    const thirdPartyCookieBlockingSettingGroup: SettingsRadioGroupElement =
        this.shadowRoot!.querySelector('#thirdPartyCookieBlockingSettingGroup')!
        ;
    const selection = Number(thirdPartyCookieBlockingSettingGroup.selected);
    if (selection === ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY) {
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.THIRD_PARTY_COOKIES_BLOCK_IN_INCOGNITO);
      this.metricsBrowserProxy_.recordAction(
            'Settings.ThirdPartyCookies.Allow');
    } else {
      assert(selection === ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY);
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.THIRD_PARTY_COOKIES_BLOCK);
      this.metricsBrowserProxy_.recordAction(
            'Settings.ThirdPartyCookies.Block');
    }

    thirdPartyCookieBlockingSettingGroup.sendPrefChange();
  }

  private relatedWebsiteSetsToggleDisabled_() {
    return this.getPref('generated.third_party_cookie_blocking_setting')
               .value !== ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY;
  }

  // SettingsViewMixin implementation.
  override getFocusConfig() {
    return new Map([
      [
        `${routes.SITE_SETTINGS_ALL.path}_${routes.COOKIES.path}`,
        '#siteDataTrigger',
      ],
    ]);
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }

  protected getCookieIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'privacy:cookie' :
        'privacy:cookie-old';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-cookies-page': SettingsCookiesPageElement;
  }
}

customElements.define(
    SettingsCookiesPageElement.is, SettingsCookiesPageElement);
