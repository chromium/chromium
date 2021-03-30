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
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import '../controls/settings_toggle_button.js';
import '../icons.js';
import '../prefs/prefs.js';
import '../settings_shared_css.js';
import '../site_settings/site_list.js';
import './collapse_radio_button.js';
import './do_not_track_toggle.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.js';
import {routes} from '../route.js';
import {Route, RouteObserverBehavior, Router} from '../router.js';
import {ContentSetting, ContentSettingsTypes} from '../site_settings/constants.js';

/**
 * The primary cookie setting associated with each radio button. Must be kept in
 * sync with the C++ enum of the same name in
 * chrome/browser/content_settings/generated_cookie_prefs.h
 * @enum {number}
 */
const CookiePrimarySetting = {
  ALLOW_ALL: 0,
  BLOCK_THIRD_PARTY_INCOGNITO: 1,
  BLOCK_THIRD_PARTY: 2,
  BLOCK_ALL: 3,
};

/**
 * Must be kept in sync with the C++ enum of the same name (see
 * chrome/browser/net/prediction_options.h).
 * @enum {number}
 */
const NetworkPredictionOptions = {
  ALWAYS: 0,
  WIFI_ONLY: 1,
  NEVER: 2,
  DEFAULT: 1,
};

Polymer({
  is: 'settings-cookies-page',

  _template: html`{__html_template__}`,

  behaviors: [
    PrefsBehavior,
    RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
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
     * @private
     */
    cookiePrimarySettingEnum_: {
      type: Object,
      value: CookiePrimarySetting,
    },

    /** @private */
    enableContentSettingsRedesign_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableContentSettingsRedesign');
      }
    },

    /**
     * Used for HTML bindings. This is defined as a property rather than
     * within the ready callback, because the value needs to be available
     * before local DOM initialization - otherwise, the toggle has unexpected
     * behavior.
     * @private {!NetworkPredictionOptions}
     */
    networkPredictionUncheckedValue_: {
      type: Number,
      value: NetworkPredictionOptions.NEVER,
    },

    /** @private */
    contentSetting_: {
      type: Object,
      value: ContentSetting,
    },

    /**
     * @private {!ContentSettingsTypes}
     */
    cookiesContentSettingType_: {
      type: String,
      value: ContentSettingsTypes.COOKIES,
    },

    /** @private */
    exceptionListsReadOnly_: {
      type: Boolean,
      value: false,
    },

    /** @private {!chrome.settingsPrivate.PrefObject} */
    blockAllPref_: {
      type: Object,
      value() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
      },
    },

    /** @type {!Map<string, (string|Function)>} */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
    },
  },

  observers: [`onGeneratedPrefsUpdated_(prefs.generated.cookie_session_only,
      prefs.generated.cookie_primary_setting)`],

  /** @type {?MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /** @override */
  ready() {
    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();
  },

  /*
   * @param {!Map<string, string>} newConfig
   * @param {?Map<string, string>} oldConfig
   * @private
   */
  focusConfigChanged_(newConfig, oldConfig) {
    assert(!oldConfig);
    assert(routes.SITE_SETTINGS_SITE_DATA);
    this.focusConfig.set(routes.SITE_SETTINGS_SITE_DATA.path, () => {
      focusWithoutInk(assert(this.$$('#site-data-trigger')));
    });
  },

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   * @protected
   */
  currentRouteChanged(route) {
    if (route !== routes.COOKIES) {
      this.$.toast.hide();
    }
  },

  /** @private */
  onSiteDataClick_() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA);
  },

  /** @private */
  onGeneratedPrefsUpdated_() {
    const sessionOnlyPref = this.getPref('generated.cookie_session_only');

    // If the clear on exit toggle is managed this implies a content setting
    // policy is present and the exception lists should be disabled.
    this.exceptionListsReadOnly_ = sessionOnlyPref.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED;

    // It is not currently possible to represent multiple management sources
    // for a single a preference. In all management scenarios, the blockAll
    // setting shares the same controlledBy as the cookie_session_only pref.
    // To support this, the controlledBy fields for the |cookie_primary_setting|
    // pref provided to the blockAll control are overwritten with values from
    // the session_only preference.
    this.set(
        'blockAllPref_',
        Object.assign(this.getPref('generated.cookie_primary_setting'), {
          controlledBy: sessionOnlyPref.controlledBy,
          controlledByName: sessionOnlyPref.controlledByName
        }));
  },

  /**
   * Record interaction metrics for the primary cookie radio setting.
   * @private
   */
  onCookiePrimarySettingChanged_() {
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
    // previously were not, and privacy sandbox APIs are enabled, the privacy
    // sandbox toast should be shown.
    const currentCookieSetting =
        this.getPref('generated.cookie_primary_setting').value;
    if (loadTimeData.getBoolean('privacySandboxSettingsEnabled') &&
        this.getPref('privacy_sandbox.apis_enabled').value &&
        (currentCookieSetting === CookiePrimarySetting.ALLOW_ALL ||
         currentCookieSetting ===
             CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO) &&
        (selection === CookiePrimarySetting.BLOCK_THIRD_PARTY ||
         selection === CookiePrimarySetting.BLOCK_ALL)) {
      this.$.toast.show();
      this.metricsBrowserProxy_.recordAction(
          'Settings.PrivacySandbox.Block3PCookies');
    } else if (
        selection === CookiePrimarySetting.ALLOW_ALL ||
        selection === CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO) {
      this.$.toast.hide();
    }

    this.$.primarySettingGroup.sendPrefChange();
  },

  /** @private */
  onClearOnExitChange_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.COOKIES_SESSION);
  },

  /**
   * Records changes made to the network prediction setting for logging, the
   * logic of actually changing the setting is taken care of by the
   * net.network_prediction_options pref.
   * @private
   */
  onNetworkPredictionChange_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.NETWORK_PREDICTION);
  },

  /** @private */
  onPrivacySandboxClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.OpenedFromCookiesPageToast');
    this.$.toast.hide();
    // TODO(crbug/1159942): Replace this with an ordinary OpenWindowProxy call.
    this.shadowRoot.getElementById('privacySandboxLink').click();
  },
});
