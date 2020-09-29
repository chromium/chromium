// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/settings_toggle_button.m.js';
import '../prefs/prefs.m.js';
import '../site_settings/settings_category_default_radio_group.js';
import '../settings_page/settings_animated_pages.m.js';
import '../settings_page/settings_subpage.m.js';
import '../settings_shared_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HatsBrowserProxyImpl} from '../hats_browser_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';
import {SyncBrowserProxyImpl, SyncStatus} from '../people_page/sync_browser_proxy.m.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.m.js';
import {routes} from '../route.js';
import {RouteObserverBehavior, Router} from '../router.m.js';
import {ChooserType, ContentSettingsTypes, CookieControlsMode} from '../site_settings/constants.js';
import {SiteSettingsPrefsBrowserProxyImpl} from '../site_settings/site_settings_prefs_browser_proxy.js';

import {PrivacyPageBrowserProxy, PrivacyPageBrowserProxyImpl} from './privacy_page_browser_proxy.m.js';

/**
 * @typedef {{
 *   enabled: boolean,
 *   pref: !chrome.settingsPrivate.PrefObject
 * }}
 */
let BlockAutoplayStatus;

Polymer({
  is: 'settings-privacy-page',

  _template: html`{__html_template__}`,

  behaviors: [
    PrefsBehavior,
    RouteObserverBehavior,
    I18nBehavior,
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
     * The current sync status, supplied by SyncBrowserProxy.
     * @type {?SyncStatus}
     */
    syncStatus: Object,

    /** @private */
    isGuest_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isGuest');
      }
    },

    /** @private */
    showClearBrowsingDataDialog_: Boolean,

    /** @private */
    enableSafeBrowsingSubresourceFilter_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter');
      }
    },

    /** @private */
    cookieSettingDescription_: String,

    /** @private */
    enableBlockAutoplayContentSetting_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableBlockAutoplayContentSetting');
      }
    },

    /** @private {BlockAutoplayStatus} */
    blockAutoplayStatus_: {
      type: Object,
      value() {
        return /** @type {BlockAutoplayStatus} */ ({});
      }
    },

    /** @private */
    enableContentSettingsRedesign_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableContentSettingsRedesign');
      }
    },

    /** @private */
    enablePaymentHandlerContentSetting_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enablePaymentHandlerContentSetting');
      }
    },

    /** @private */
    enableExperimentalWebPlatformFeatures_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures');
      },
    },

    /** @private */
    enableSecurityKeysSubpage_: {
      type: Boolean,
      readOnly: true,
      value() {
        return loadTimeData.getBoolean('enableSecurityKeysSubpage');
      }
    },

    /** @private */
    enableFontAccessContentSetting_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableFontAccessContentSetting');
      }
    },

    /** @private */
    enableQuietNotificationPromptsSetting_: {
      type: Boolean,
      value: () =>
          loadTimeData.getBoolean('enableQuietNotificationPromptsSetting'),
    },

    /** @private */
    enableWebBluetoothNewPermissionsBackend_: {
      type: Boolean,
      value: () =>
          loadTimeData.getBoolean('enableWebBluetoothNewPermissionsBackend'),
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();

        if (routes.SECURITY) {
          map.set(routes.SECURITY.path, '#securityLinkRow');
        }

        if (routes.COOKIES) {
          map.set(
              `${routes.COOKIES.path}_${routes.PRIVACY.path}`,
              '#cookiesLinkRow');
          map.set(
              `${routes.COOKIES.path}_${routes.BASIC.path}`, '#cookiesLinkRow');
        }

        if (routes.SITE_SETTINGS) {
          map.set(routes.SITE_SETTINGS.path, '#permissionsLinkRow');
        }

        return map;
      },
    },

    /** @private */
    searchFilter_: String,

    /** @private */
    siteDataFilter_: String,

    /**
     * Boolean which keeps a track if any of the displayed lists has discarded
     * content setting patterns and hides the warning message accordingly.
     * @private
     */
    pluginsHasDiscardedExceptions_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {?PrivacyPageBrowserProxy} */
  browserProxy_: null,

  /** @private {?MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /** @override */
  ready() {
    this.ContentSettingsTypes = ContentSettingsTypes;
    this.ChooserType = ChooserType;

    this.browserProxy_ = PrivacyPageBrowserProxyImpl.getInstance();
    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();

    this.onBlockAutoplayStatusChanged_({
      pref: /** @type {chrome.settingsPrivate.PrefObject} */ ({value: false}),
      enabled: false
    });

    this.addWebUIListener(
        'onBlockAutoplayStatusChanged',
        this.onBlockAutoplayStatusChanged_.bind(this));

    SyncBrowserProxyImpl.getInstance().getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));

    SiteSettingsPrefsBrowserProxyImpl.getInstance()
        .getCookieSettingDescription()
        .then(description => this.cookieSettingDescription_ = description);
    this.addWebUIListener(
        'cookieSettingDescriptionChanged',
        description => this.cookieSettingDescription_ = description);
  },

  /**
   * Handler for when the sync state is pushed from the browser.
   * @param {?SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_(syncStatus) {
    this.syncStatus = syncStatus;
  },

  /** @protected */
  currentRouteChanged() {
    this.showClearBrowsingDataDialog_ =
        Router.getInstance().getCurrentRoute() === routes.CLEAR_BROWSER_DATA;
  },

  /**
   * Called when the block autoplay status changes.
   * @param {BlockAutoplayStatus} autoplayStatus
   * @private
   */
  onBlockAutoplayStatusChanged_(autoplayStatus) {
    this.blockAutoplayStatus_ = autoplayStatus;
  },

  /**
   * Updates the block autoplay pref when the toggle is changed.
   * @param {!Event} event
   * @private
   */
  onBlockAutoplayToggleChange_(event) {
    const target = /** @type {!SettingsToggleButtonElement} */ (event.target);
    this.browserProxy_.setBlockAutoplayEnabled(target.checked);
  },

  /**
   * This is a workaround to connect the remove all button to the subpage.
   * @private
   */
  onRemoveAllCookiesFromSite_() {
    const node = /** @type {?SiteDataDetailsSubpageElement} */ (
        this.$$('site-data-details-subpage'));
    if (node) {
      node.removeAll();
    }
  },

  /** @private */
  onClearBrowsingDataTap_() {
    this.tryShowHatsSurvey_();

    Router.getInstance().navigateTo(routes.CLEAR_BROWSER_DATA);
  },

  /** @private */
  onCookiesClick_() {
    this.tryShowHatsSurvey_();

    Router.getInstance().navigateTo(routes.COOKIES);
  },

  /** @private */
  onDialogClosed_() {
    Router.getInstance().navigateTo(assert(routes.CLEAR_BROWSER_DATA.parent));
    focusWithoutInk(assert(this.$$('#clearBrowsingData')));
  },

  /** @private */
  onPermissionsPageClick_() {
    this.tryShowHatsSurvey_();

    Router.getInstance().navigateTo(routes.SITE_SETTINGS);
  },

  /** @private */
  onSecurityPageClick_() {
    this.tryShowHatsSurvey_();
    this.metricsBrowserProxy_.recordAction(
        'SafeBrowsing.Settings.ShowedFromParentSettings');
    Router.getInstance().navigateTo(routes.SECURITY);
  },

  /** @private */
  getProtectedContentLabel_(value) {
    return value ? this.i18n('siteSettingsProtectedContentEnable') :
                   this.i18n('siteSettingsBlocked');
  },

  /** @private */
  getProtectedContentIdentifiersLabel_(value) {
    return value ? this.i18n('siteSettingsProtectedContentEnableIdentifiers') :
                   this.i18n('siteSettingsBlocked');
  },

  /** @private */
  tryShowHatsSurvey_() {
    HatsBrowserProxyImpl.getInstance().tryShowSurvey();
  },
});
