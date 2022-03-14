// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-cookies-page' is the settings page containing cookies
 * settings.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import '../controls/settings_toggle_button.js';
import '../icons.js';
import '../prefs/prefs.js';
import '../settings_shared_css.js';
import '../site_settings/site_list.js';
import './collapse_radio_button.js';
import './do_not_track_toggle.js';
import '../controls/settings_radio_group.js';

import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
import {WebUIListenerMixin, WebUIListenerMixinInterface} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';
import {PrefsMixin, PrefsMixinInterface} from '../prefs/prefs_mixin.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';
import {ContentSetting, ContentSettingsTypes} from '../site_settings/constants.js';
import {CookiePrimarySetting} from '../site_settings/site_settings_prefs_browser_proxy.js';

import {SettingsCollapseRadioButtonElement} from './collapse_radio_button.js';
import {getTemplate} from './cookies_page.html.js';

/**
 * Must be kept in sync with the C++ enum of the same name (see
 * chrome/browser/prefetch/prefetch_prefs.h).
 */
enum NetworkPredictionOptions {
  STANDARD = 0,
  WIFI_ONLY_DEPRECATED = 1,
  DISABLED = 2,
  EXTENDED = 3,
  DEFAULT = 1,
}

type FocusConfig = Map<string, (string|(() => void))>;

export interface SettingsCookiesPageElement {
  $: {
    allowAll: SettingsCollapseRadioButtonElement,
    blockAll: SettingsCollapseRadioButtonElement,
    blockThirdPartyIncognito: SettingsCollapseRadioButtonElement,
    blockThirdParty: SettingsCollapseRadioButtonElement,
    primarySettingGroup: SettingsRadioGroupElement,
    toast: CrToastElement,
  };
}

const SettingsCookiesPageElementBase =
    RouteObserverMixin(
        WebUIListenerMixin(I18nMixin(PrefsMixin(PolymerElement)))) as {
      new ():
          PolymerElement & I18nMixinInterface & WebUIListenerMixinInterface &
      PrefsMixinInterface & RouteObserverMixinInterface
    };

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
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Current search term.
       */
      searchTerm: {
        type: String,
        notify: true,
        value: '',
      },

      /**
       * Primary cookie control states for use in bindings.
       */
      cookiePrimarySettingEnum_: {
        type: Object,
        value: CookiePrimarySetting,
      },

      /**
       * Used for HTML bindings. This is defined as a property rather than
       * within the ready callback, because the value needs to be available
       * before local DOM initialization - otherwise, the toggle has unexpected
       * behavior.
       */
      networkPredictionUncheckedValue_: {
        type: Number,
        value: NetworkPredictionOptions.DISABLED,
      },

      contentSetting_: {
        type: Object,
        value: ContentSetting,
      },

      cookiesContentSettingType_: {
        type: String,
        value: ContentSettingsTypes.COOKIES,
      },

      exceptionListsReadOnly_: {
        type: Boolean,
        value: false,
      },

      blockAllPref_: {
        type: Object,
        value() {
          return {};
        },
      },

      enableConsolidatedSiteStorageControls_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('consolidatedSiteStorageControlsEnabled'),
      },

      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },
    };
  }

  static get observers() {
    return [`onGeneratedPrefsUpdated_(prefs.generated.cookie_session_only,
        prefs.generated.cookie_primary_setting)`];
  }

  searchTerm: string;
  private cookiesContentSettingType_: ContentSettingsTypes;
  private exceptionListsReadOnly_: boolean;
  private blockAllPref_: chrome.settingsPrivate.PrefObject;
  private enableConsolidatedSiteStorageControls_: boolean;
  focusConfig: FocusConfig;

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  private focusConfigChanged_(_newConfig: FocusConfig, oldConfig: FocusConfig) {
    assert(!oldConfig);
    assert(
        this.enableConsolidatedSiteStorageControls_ ?
            routes.SITE_SETTINGS_ALL :
            routes.SITE_SETTINGS_SITE_DATA);
    const selectSiteDataLinkRow = () => {
      focusWithoutInk(
          assert(this.shadowRoot!.querySelector('#site-data-trigger')!));
    };
    if (this.enableConsolidatedSiteStorageControls_) {
      this.focusConfig.set(
          `${routes.SITE_SETTINGS_ALL.path}_${routes.COOKIES.path}`,
          selectSiteDataLinkRow);
    } else {
      this.focusConfig.set(
          routes.SITE_SETTINGS_SITE_DATA.path, selectSiteDataLinkRow);
    }
  }

  override currentRouteChanged(route: Route) {
    if (route !== routes.COOKIES) {
      this.$.toast.hide();
    }
  }

  private getSiteDataLabel_(): string {
    return this.enableConsolidatedSiteStorageControls_ ?
        this.i18n('cookiePageAllSitesLink') :
        this.i18n('siteSettingsCookieLink');
  }

  private onSiteDataClick_() {
    if (this.enableConsolidatedSiteStorageControls_) {
      Router.getInstance().navigateTo(routes.SITE_SETTINGS_ALL);
    } else {
      Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA);
    }
  }

  private onGeneratedPrefsUpdated_() {
    const sessionOnlyPref = this.getPref('generated.cookie_session_only');

    // If the clear on exit toggle is managed this implies a content setting
    // policy is present and the exception lists should be disabled.
    this.exceptionListsReadOnly_ = sessionOnlyPref.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED;

    // It is not currently possible to represent multiple management
    // sources for a single a preference. In all management scenarios,
    // the blockAll setting shares the same controlledBy as the
    // cookie_session_only pref. To support this, the controlledBy
    // fields for the |cookie_primary_setting| pref provided to the
    // blockAll control are overwritten with values from the session_only
    // preference.
    this.set(
        'blockAllPref_',
        Object.assign(this.getPref('generated.cookie_primary_setting'), {
          controlledBy: sessionOnlyPref.controlledBy,
          controlledByName: sessionOnlyPref.controlledByName
        }));
  }

  /**
   * Record interaction metrics for the primary cookie radio setting.
   */
  private onCookiePrimarySettingChanged_() {
    const selection = Number(this.$.primarySettingGroup.selected);
    if (selection === CookiePrimarySetting.ALLOW_ALL) {
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.COOKIES_ALL);
    } else if (selection === CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO) {
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.COOKIES_INCOGNITO);
    } else if (selection === CookiePrimarySetting.BLOCK_THIRD_PARTY) {
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.COOKIES_THIRD);
    } else {  // CookiePrimarySetting.BLOCK_ALL
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.COOKIES_BLOCK);
    }

    // If this change resulted in the user now blocking 3P cookies where they
    // previously were not, and privacy sandbox APIs are enabled,
    // the privacy sandbox toast should be shown.
    const currentCookieSetting =
        this.getPref('generated.cookie_primary_setting').value;
    const privacySandboxEnabled =
        loadTimeData.getBoolean('privacySandboxSettings3Enabled') ?
        this.getPref('privacy_sandbox.apis_enabled_v2').value :
        this.getPref('privacy_sandbox.apis_enabled').value;

    if (privacySandboxEnabled &&
        (currentCookieSetting === CookiePrimarySetting.ALLOW_ALL ||
         currentCookieSetting ===
             CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO) &&
        (selection === CookiePrimarySetting.BLOCK_THIRD_PARTY ||
         selection === CookiePrimarySetting.BLOCK_ALL)) {
      if (!loadTimeData.getBoolean('isPrivacySandboxRestricted')) {
        this.$.toast.show();
      }
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacySandbox.Block3PCookies');
    } else if (
        selection === CookiePrimarySetting.ALLOW_ALL ||
        selection === CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO) {
      this.$.toast.hide();
    }

    this.$.primarySettingGroup.sendPrefChange();
  }

  private onClearOnExitChange_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.COOKIES_SESSION);
  }

  /**
   * Records changes made to the network prediction setting for logging, the
   * logic of actually changing the setting is taken care of by the
   * net.network_prediction_options pref.
   */
  private onNetworkPredictionChange_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.NETWORK_PREDICTION);
  }

  private onPrivacySandboxClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.OpenedFromCookiesPageToast');
    this.$.toast.hide();
    // TODO(crbug/1159942): Replace this with an ordinary OpenWindowProxy call.
    this.shadowRoot!.querySelector<HTMLAnchorElement>(
                        '#privacySandboxLink')!.click();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-cookies-page': SettingsCookiesPageElement;
  }
}

customElements.define(
    SettingsCookiesPageElement.is, SettingsCookiesPageElement);
