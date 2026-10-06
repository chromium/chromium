// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-cookies-page' is the settings page containing cookies
 * settings.
 */

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/collapse_radio_button.js';
import '../controls/controlled_radio_button.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../privacy_icons.html.js';
import '../settings_page/settings_subpage.js';
import '../site_settings/site_list.js';
import './do_not_track_toggle.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {Router} from '../router.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';
import {ThirdPartyCookieBlockingSetting} from '../site_settings/site_settings_browser_proxy.js';

import {getCss} from './cookies_page.css.js';
import {getHtml} from './cookies_page.html.js';

const SettingsCookiesPageElementBase =
    SettingsViewMixinLit(WebUiListenerMixinLit(I18nMixinLit(CrLitElement)));

export type CookiesPageElement = SettingsCookiesPageElement;

export class SettingsCookiesPageElement extends SettingsCookiesPageElementBase {
  static get is() {
    return 'settings-cookies-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Current search term.
       */
      searchTerm: {
        type: String,
        notify: true,
      },

      showUniversalOptOutSettings_: {type: Boolean},

      pageTitle_: {type: String},

      isSettingsRefresh2026_: {type: Boolean},
    };
  }

  accessor searchTerm: string = '';
  protected accessor pageTitle_: string = '';
  protected accessor showUniversalOptOutSettings_: boolean =
      loadTimeData.getBoolean('showUniversalOptOutSettings');
  protected accessor isSettingsRefresh2026_: boolean =
      loadTimeData.getString('settingsRefresh2026') !== '';

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('showUniversalOptOutSettings_')) {
      this.pageTitle_ = this.computePageTitle_();
    }
  }

  protected onSearchTermChanged_(e: CustomEvent<{value: string}>) {
    this.searchTerm = e.detail.value;
  }

  protected onSiteDataClick_() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_ALL);
  }

  protected onThirdPartyCookieBlockingSettingGroupChange_() {
    const thirdPartyCookieBlockingSettingGroup: SettingsRadioGroupElement =
        this.shadowRoot.querySelector('#thirdPartyCookieBlockingSettingGroup')!;
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

  protected onUniversalOptOutToggleSettingsBooleanControlChange_(event: Event) {
    const toggle = event.target as SettingsToggleButtonElement;

    this.metricsBrowserProxy_.recordAction(
        toggle.checked ? 'Privacy.UniversalOptOut.SettingsToggleOn' :
                         'Privacy.UniversalOptOut.SettingsToggleOff');
  }

  private computePageTitle_(): string {
    return this.i18n(
        this.showUniversalOptOutSettings_ ?
            'thirdPartyCookiesAndSiteDataPageTitle' :
            'thirdPartyCookiesPageTitle');
  }

  override currentRouteChanged(currentRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(currentRoute, oldRoute);

    if (currentRoute === routes.COOKIES) {
      this.metricsBrowserProxy_.recordBooleanHistogram(
          'Privacy.UniversalOptOut.SettingsVisibility',
          this.showUniversalOptOutSettings_);
    }
  }

  // SettingsViewMixinLit implementation.
  override getFocusConfig() {
    return new Map([
      [
        `${routes.SITE_SETTINGS_ALL.path}_${routes.COOKIES.path}`,
        '#siteDataTrigger',
      ],
    ]);
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-cookies-page': SettingsCookiesPageElement;
  }
}

customElements.define(
    SettingsCookiesPageElement.is, SettingsCookiesPageElement);
