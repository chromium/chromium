// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './battery_status_card.js';
import './cpu_card.js';
import './diagnostics_shared_css.js';
import './icons.js';
import './memory_card.js';
import './overview_card.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiagnosticsBrowserProxy, DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {SystemDataProviderInterface, SystemInfo} from './diagnostics_types.js';
import {getSystemDataProvider} from './mojo_interface_provider.js';
import {TestSuiteStatus} from './routine_list_executor.js';

/**
 * @fileoverview
 * 'system-page' is the main page for viewing telemetric system information
 * and running diagnostic tests.
 */
Polymer({
  is: 'system-page',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @private {?SystemDataProviderInterface}
   */
  systemDataProvider_: null,

  /**
   * @private {?DiagnosticsBrowserProxy}
   */
  browserProxy_: null,

  properties: {
    /** @protected {boolean} */
    saveSessionLogEnabled_: {
      type: Boolean,
      value: true,
    },

    /** @private {boolean} */
    showBatteryStatusCard_: {
      type: Boolean,
      value: false,
    },

    /** @type {!TestSuiteStatus} */
    testSuiteStatus: {
      type: Number,
      value: TestSuiteStatus.kNotRunning,
    },

    /** @type {boolean} */
    systemInfoReceived_: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    toastText_: {
      type: String,
      value: '',
    },

    /** @private {boolean} */
    isLoggedIn_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isLoggedIn'),
    },

    /** @type {string} */
    bannerMessage: {
      type: String,
      value: '',
    },

    /** @private {string} */
    scrollingClass_: {
      type: String,
      value: '',
    },

    /** @private {number} */
    scrollTimerId_: {
      type: Number,
      value: -1,
    },

    /** @type {boolean} */
    isActive: {
      type: Boolean,
      value: true,
    },

    /** @type {boolean} */
    isNetworkingEnabled: {
      type: Boolean,
      value: loadTimeData.getBoolean('isNetworkingEnabled'),
    },
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.fetchSystemInfo_();
    this.browserProxy_ = DiagnosticsBrowserProxyImpl.getInstance();
    this.browserProxy_.initialize();

    // Only use inner banner behavior if system page is in stand-alone mode.
    if (!this.isNetworkingEnabled) {
      this.addCautionBannerEventListeners_();
    }
  },

  /** @private */
  fetchSystemInfo_() {
    this.systemDataProvider_.getSystemInfo().then((result) => {
      this.onSystemInfoReceived_(result.systemInfo);
    });
    setTimeout(() => this.recordLateSystemInfo_(), 3000);
  },

  /**
   * @param {!SystemInfo} systemInfo
   * @private
   */
  onSystemInfoReceived_(systemInfo) {
    this.systemInfoReceived_ = true;
    this.showBatteryStatusCard_ = systemInfo.deviceCapabilities.hasBattery;
  },

  /**
   * @private
   */
  recordLateSystemInfo_() {
    if (!this.systemInfoReceived_) {
      console.warn('system info not received within three seconds.');
    }
  },

  /** @protected */
  onSessionLogClick_() {
    // Click already handled then leave early.
    if (!this.saveSessionLogEnabled_) {
      return;
    }

    this.saveSessionLogEnabled_ = false;
    this.browserProxy_.saveSessionLog()
        .then(
            /* @type {boolean} */ (success) => {
              const result = success ? 'Success' : 'Failure';
              this.toastText_ =
                  loadTimeData.getString(`sessionLogToastText${result}`);
              this.$.toast.show();
            })
        .catch(() => {/* File selection cancelled */})
        .finally(() => {
          this.saveSessionLogEnabled_ = true;
        });
  },

  /** @private */
  addCautionBannerEventListeners_() {
    window.addEventListener('show-caution-banner', (e) => {
      assert(e.detail.message);
      this.bannerMessage = e.detail.message;
    });

    window.addEventListener('dismiss-caution-banner', () => {
      this.bannerMessage = '';
    });

    window.addEventListener('scroll', () => {
      if (!this.bannerMessage) {
        return;
      }

      // Reset timer since we've received another 'scroll' event.
      if (this.scrollTimerId_ !== -1) {
        this.scrollingClass_ = 'elevation-2';
        clearTimeout(this.scrollTimerId_);
      }

      // Remove box shadow from banner since the user has stopped scrolling
      // for at least 300ms.
      this.scrollTimerId_ =
          window.setTimeout(() => this.scrollingClass_ = '', 300);
    });
  },

  /**
   * 'navigation-view-panel' is responsible for calling this function when
   * the active page changes.
   * @param {{isActive: boolean}} isActive
   * @public
   */
  onNavigationPageChanged({isActive}) {
    this.isActive = isActive;
    if (isActive) {
      // Focus the topmost system page element.
      this.$$('#overviewCard').$$('#overviewCardContainer').focus();
      // TODO(ashleydp): Remove when a call can be made at a higher component
      // to avoid duplicate code in all navigatable pages.
      this.browserProxy_.recordNavigation('system');
    }
  },

  /**
   * @protected
   * @return {string}
   */
  getCardContainerClass_() {
    const cardContainer = 'diagnostics-cards-container';
    return `${cardContainer}${this.isNetworkingEnabled ? '-nav' : ''}`;
  },
});
