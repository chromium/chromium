// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-details' show the details (permissions and usage) for a given origin
 * under Site Settings.
 */
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../icons.js';
import '../settings_shared_css.js';
import './all_sites_icons.js';
import './clear_storage_dialog_css.js';
import './site_details_permission.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Route, RouteObserverBehavior, Router} from '../router.js';

import {ContentSetting, ContentSettingsTypes} from './constants.js';
import {SiteSettingsBehavior} from './site_settings_behavior.js';
import {WebsiteUsageBrowserProxy, WebsiteUsageBrowserProxyImpl} from './website_usage_browser_proxy.js';

Polymer({
  is: 'site-details',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior, SiteSettingsBehavior, RouteObserverBehavior,
    WebUIListenerBehavior
  ],

  properties: {
    /**
     * Whether unified autoplay blocking is enabled.
     */
    blockAutoplayEnabled: Boolean,

    /**
     * Use the string representing the origin or extension name as the page
     * title of the settings-subpage parent.
     */
    pageTitle: {
      type: String,
      notify: true,
    },

    /**
     * The origin that this widget is showing details for.
     * @private
     */
    origin_: String,

    /**
     * The amount of data stored for the origin.
     * @private
     */
    storedData_: {
      type: String,
      value: '',
    },

    /**
     * The number of cookies stored for the origin.
     * @private
     */
    numCookies_: {
      type: String,
      value: '',
    },

    /** @private */
    enableExperimentalWebPlatformFeatures_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures');
      },
    },

    /** @private */
    enableFontAccessContentSetting_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableFontAccessContentSetting');
      }
    },

    /** @private */
    enableWebBluetoothNewPermissionsBackend_: {
      type: Boolean,
      value: () =>
          loadTimeData.getBoolean('enableWebBluetoothNewPermissionsBackend'),
    },

    /** @private */
    enableFileHandlingContentSetting_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableFileHandlingContentSetting');
      }
    },
  },

  /** @private {string} */
  fetchingForHost_: '',

  /** @private {?WebsiteUsageBrowserProxy} */
  websiteUsageProxy_: null,

  /** @override */
  attached() {
    this.websiteUsageProxy_ = WebsiteUsageBrowserProxyImpl.getInstance();
    this.addWebUIListener('usage-total-changed', (host, data, cookies) => {
      this.onUsageTotalChanged_(host, data, cookies);
    });

    this.addWebUIListener(
        'contentSettingSitePermissionChanged',
        this.onPermissionChanged_.bind(this));

    // <if expr="chromeos">
    this.addWebUIListener(
        'prefEnableDrmChanged', this.prefEnableDrmChanged_.bind(this));
    // </if>

    // Refresh block autoplay status from the backend.
    this.browserProxy.fetchBlockAutoplayStatus();
  },

  /** @override */
  ready() {
    this.ContentSettingsTypes = ContentSettingsTypes;
  },

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   * @protected
   */
  currentRouteChanged(route) {
    if (route !== routes.SITE_SETTINGS_SITE_DETAILS) {
      return;
    }
    const site = Router.getInstance().getQueryParameters().get('site');
    if (!site) {
      return;
    }
    this.origin_ = site;
    this.browserProxy.isOriginValid(this.origin_).then((valid) => {
      if (!valid) {
        Router.getInstance().navigateToPreviousRoute();
      } else {
        this.fetchingForHost_ = this.toUrl(this.origin_).hostname;
        this.storedData_ = '';
        this.websiteUsageProxy_.fetchUsageTotal(this.fetchingForHost_);
        this.updatePermissions_(this.getCategoryList());
      }
    });
  },

  /**
   * Called when a site within a category has been changed.
   * @param {!ContentSettingsTypes} category The category that
   *     changed.
   * @param {string} origin The origin of the site that changed.
   * @param {string} embeddingOrigin The embedding origin of the site that
   *     changed.
   * @private
   */
  onPermissionChanged_(category, origin, embeddingOrigin) {
    if (this.origin_ === undefined || this.origin_ === '' ||
        origin === undefined || origin === '') {
      return;
    }
    if (!this.getCategoryList().includes(category)) {
      return;
    }

    // Site details currently doesn't support embedded origins, so ignore it
    // and just check whether the origins are the same.
    this.updatePermissions_([category]);
  },

  /**
   * Callback for when the usage total is known.
   * @param {string} host The host that the usage was fetched for.
   * @param {string} usage The string showing how much data the given host
   *     is using.
   * @param {string} cookies The string showing how many cookies the given host
   *     is using.
   * @private
   */
  onUsageTotalChanged_(host, usage, cookies) {
    if (this.fetchingForHost_ === host) {
      this.storedData_ = usage;
      this.numCookies_ = cookies;
    }
  },

  // <if expr="chromeos">
  prefEnableDrmChanged_() {
    this.updatePermissions_([ContentSettingsTypes.PROTECTED_CONTENT]);
  },
  // </if>

  /**
   * Retrieves the permissions listed in |categoryList| from the backend for
   * |this.origin_|.
   * @param {!Array<!ContentSettingsTypes>} categoryList The list
   *     of categories to update permissions for.
   * @private
   */
  updatePermissions_(categoryList) {
    const permissionsMap =
        /**
         * @type {!Object<!ContentSettingsTypes,
         *         !SiteDetailsPermissionElement>}
         */
        (Array.prototype.reduce.call(
            this.root.querySelectorAll('site-details-permission'),
            (map, element) => {
              if (categoryList.includes(element.category)) {
                map[element.category] = element;
              }
              return map;
            },
            {}));

    this.browserProxy.getOriginPermissions(this.origin_, categoryList)
        .then((exceptionList) => {
          exceptionList.forEach((exception, i) => {
            // |exceptionList| should be in the same order as
            // |categoryList|.
            if (permissionsMap[categoryList[i]]) {
              permissionsMap[categoryList[i]].site = exception;
            }
          });

          // The displayName won't change, so just use the first
          // exception.
          assert(exceptionList.length > 0);
          this.pageTitle =
              this.originRepresentation(exceptionList[0].displayName);
        });
  },

  /** @private */
  onCloseDialog_(e) {
    e.target.closest('cr-dialog').close();
  },

  /**
   * Confirms the resetting of all content settings for an origin.
   * @param {!Event} e
   * @private
   */
  onConfirmClearSettings_(e) {
    e.preventDefault();
    this.$.confirmResetSettings.showModal();
  },

  /**
   * Confirms the clearing of storage for an origin.
   * @param {!Event} e
   * @private
   */
  onConfirmClearStorage_(e) {
    e.preventDefault();
    this.$.confirmClearStorageNew.showModal();
  },

  /**
   * Resets all permissions for the current origin.
   * @private
   */
  onResetSettings_(e) {
    this.browserProxy.setOriginPermissions(
        this.origin_, this.getCategoryList(), ContentSetting.DEFAULT);

    this.onCloseDialog_(e);
  },

  /**
   * Clears all data stored, except cookies, for the current origin.
   * @private
   */
  onClearStorage_(e) {
    MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
        PrivacyElementInteractions.SITE_DETAILS_CLEAR_DATA);
    if (this.hasUsage_(this.storedData_, this.numCookies_)) {
      this.websiteUsageProxy_.clearUsage(this.toUrl(this.origin_).href);
      this.storedData_ = '';
      this.numCookies_ = '';
    }

    this.onCloseDialog_(e);
  },

  /**
   * Checks whether this site has any usage information to show.
   * @return {boolean} Whether there is any usage information to show (e.g.
   *     disk or battery).
   * @private
   */
  hasUsage_(storage, cookies) {
    return storage !== '' || cookies !== '';
  },

  /**
   * Checks whether this site has both storage and cookies information to show.
   * @return {boolean} Whether there are both storage and cookies information to
   *     show.
   * @private
   */
  hasDataAndCookies_(storage, cookies) {
    return storage !== '' && cookies !== '';
  },

  /** @private */
  onResetSettingsDialogClosed_() {
    focusWithoutInk(assert(this.$$('#resetSettingsButton')));
  },

  /** @private */
  onClearStorageDialogClosed_() {
    focusWithoutInk(assert(this.$$('#clearStorage')));
  },
});
