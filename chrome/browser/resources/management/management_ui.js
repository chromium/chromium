// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_page_host_style_css.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './icons.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserReportingResponse, Extension, ManagementBrowserProxy, ManagementBrowserProxyImpl, ReportingType, ThreatProtectionInfo} from './management_browser_proxy.js';
// <if expr="chromeos">
import {DeviceReportingResponse, DeviceReportingType} from './management_browser_proxy.js';
// </if>

/**
 * @typedef {{
 *   messageIds: !Array<string>,
 *   icon: string,
 * }}
 */
let BrowserReportingData;

Polymer({
  is: 'management-ui',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * List of messages related to browser reporting.
     * @private {?Array<!BrowserReportingData>}
     */
    browserReportingInfo_: Array,

    /**
     * List of messages related to browser reporting.
     * @private {?Array<!Extension>}
     */
    extensions_: Array,

    /**
     * List of messages related to browser reporting.
     * @private (?Array<!String>)
     */
    managedWebsites_: Array,

    // <if expr="chromeos">
    /**
     * List of messages related to device reporting.
     * @private {?Array<!DeviceReportingResponse>}
     */
    deviceReportingInfo_: Array,

    /**
     * Message stating if the Trust Roots are configured.
     * @private
     */
    localTrustRoots_: String,

    /** @private */
    customerLogo_: String,

    /** @private */
    managementOverview_: String,

    /** @private */
    pluginVmDataCollectionEnabled_: Boolean,

    /** @private */
    eolAdminMessage_: String,

    /** @private */
    eolMessage_: String,

    /** @private */
    showProxyServerPrivacyDisclosure_: Boolean,

    // </if>

    /** @private */
    subtitle_: String,

    // <if expr="not chromeos">
    /** @private */
    managementNoticeHtml_: String,
    // </if>

    /** @private */
    managed_: Boolean,

    /** @private */
    extensionReportingSubtitle_: String,

    /** @private {!ThreatProtectionInfo} */
    threatProtectionInfo_: Object,
  },

  /** @private {?ManagementBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached() {
    document.documentElement.classList.remove('loading');
    this.browserProxy_ = ManagementBrowserProxyImpl.getInstance();
    this.updateManagedFields_();
    this.initBrowserReportingInfo_();
    this.getThreatProtectionInfo_();

    this.addWebUIListener(
        'browser-reporting-info-updated',
        reportingInfo => this.onBrowserReportingInfoReceived_(reportingInfo));

    this.addWebUIListener(
        'plugin-vm-data-collection-updated',
        enabled => this.pluginVmDataCollectionEnabled_ = enabled);

    this.addWebUIListener('managed_data_changed', () => {
      this.updateManagedFields_();
    });

    this.addWebUIListener(
        'threat-protection-info-updated',
        info => this.threatProtectionInfo_ = info);

    this.getExtensions_();
    this.getManagedWebsites_();
    // <if expr="chromeos">
    this.getDeviceReportingInfo_();
    this.getPluginVmDataCollectionStatus_();
    this.getLocalTrustRootsInfo_();
    // </if>
  },

  /** @private */
  initBrowserReportingInfo_() {
    this.browserProxy_.initBrowserReportingInfo().then(
        reportingInfo => this.onBrowserReportingInfoReceived_(reportingInfo));
  },

  /**
   * @param {!Array<!BrowserReportingResponse>} reportingInfo
   * @private
   */
  onBrowserReportingInfoReceived_(reportingInfo) {
    const reportingInfoMap = reportingInfo.reduce((info, response) => {
      info[response.reportingType] = info[response.reportingType] || {
        icon: this.getIconForReportingType_(response.reportingType),
        messageIds: []
      };
      info[response.reportingType].messageIds.push(response.messageId);
      return info;
    }, {});

    const reportingTypeOrder = {
      [ReportingType.SECURITY]: 1,
      [ReportingType.EXTENSIONS]: 2,
      [ReportingType.USER]: 3,
      [ReportingType.USER_ACTIVITY]: 4,
      [ReportingType.DEVICE]: 5,
    };

    this.browserReportingInfo_ =
        Object.keys(reportingInfoMap)
            .sort((a, b) => reportingTypeOrder[a] - reportingTypeOrder[b])
            .map(reportingType => reportingInfoMap[reportingType]);
  },

  /** @private */
  getExtensions_() {
    this.browserProxy_.getExtensions().then(extensions => {
      this.extensions_ = extensions;
    });
  },

  /** @private */
  getManagedWebsites_() {
    this.browserProxy_.getManagedWebsites().then(managedWebsites => {
      this.managedWebsites_ = managedWebsites;
    });
  },

  /** @private */
  getThreatProtectionInfo_() {
    this.browserProxy_.getThreatProtectionInfo().then(info => {
      this.threatProtectionInfo_ = info;
    });
  },

  /**
   * @return {boolean} True if there is threat protection info to show.
   * @private
   */
  showThreatProtectionInfo_() {
    return !!this.threatProtectionInfo_ &&
        this.threatProtectionInfo_.info.length > 0;
  },

  // <if expr="chromeos">
  /** @private */
  getLocalTrustRootsInfo_() {
    this.browserProxy_.getLocalTrustRootsInfo().then(trustRootsConfigured => {
      this.localTrustRoots_ = trustRootsConfigured ?
          loadTimeData.getString('managementTrustRootsConfigured') :
          '';
    });
  },

  /** @private */
  getDeviceReportingInfo_() {
    this.browserProxy_.getDeviceReportingInfo().then(reportingInfo => {
      this.deviceReportingInfo_ = reportingInfo;
    });
  },

  /** @private */
  getPluginVmDataCollectionStatus_() {
    this.browserProxy_.getPluginVmDataCollectionStatus().then(
        pluginVmDataCollectionEnabled => {
          this.pluginVmDataCollectionEnabled_ = pluginVmDataCollectionEnabled;
        });
  },

  /**
   * @return {boolean} True of there are device reporting info to show.
   * @private
   */
  showDeviceReportingInfo_() {
    return !!this.deviceReportingInfo_ && this.deviceReportingInfo_.length > 0;
  },

  /**
   * @param {string} eolAdminMessage The device return instructions
   * @return {boolean} Whether there are device return instructions from the
   *     admin in case an update is required after reaching end of life.
   * @private
   */
  isEmpty_(eolAdminMessage) {
    return !eolAdminMessage || eolAdminMessage.trim().length === 0;
  },

  /**
   * @param {DeviceReportingType} reportingType
   * @return {string} The associated icon.
   * @private
   */
  getIconForDeviceReportingType_(reportingType) {
    switch (reportingType) {
      case DeviceReportingType.SUPERVISED_USER:
        return 'management:supervised-user';
      case DeviceReportingType.DEVICE_ACTIVITY:
        return 'management:timelapse';
      case DeviceReportingType.STATISTIC:
        return 'management:bar-chart';
      case DeviceReportingType.DEVICE:
        return 'cr:computer';
      case DeviceReportingType.CRASH_REPORT:
        return 'management:crash';
      case DeviceReportingType.APP_INFO_AND_ACTIVITY:
        return 'management:timelapse';
      case DeviceReportingType.LOGS:
        return 'management:report';
      case DeviceReportingType.PRINT:
        return 'cr:print';
      case DeviceReportingType.PRINT_JOBS:
        return 'cr:print';
      case DeviceReportingType.CROSTINI:
        return 'management:linux';
      case DeviceReportingType.USERNAME:
        return 'management:account-circle';
      case DeviceReportingType.EXTENSION:
        return 'cr:extension';
      case DeviceReportingType.ANDROID_APPLICATION:
        return 'management:play-store';
      default:
        return 'cr:computer';
    }
  },
  // </if>

  /**
   * @return {boolean} True of there are browser reporting info to show.
   * @private
   */
  showBrowserReportingInfo_() {
    return !!this.browserReportingInfo_ &&
        this.browserReportingInfo_.length > 0;
  },

  /**
   * @return {boolean} True of there are extension reporting info to show.
   * @private
   */
  showExtensionReportingInfo_() {
    return !!this.extensions_ && this.extensions_.length > 0;
  },

  /**
   * @return {boolean} True of there is managed websites info to show.
   * @private
   */
  showManagedWebsitesInfo_() {
    return !!this.managedWebsites_ && this.managedWebsites_.length > 0;
  },


  /**
   * @param {ReportingType} reportingType
   * @returns {string} The associated icon.
   * @private
   */
  getIconForReportingType_(reportingType) {
    switch (reportingType) {
      case ReportingType.SECURITY:
        return 'cr:security';
      case ReportingType.DEVICE:
        return 'cr:computer';
      case ReportingType.EXTENSIONS:
        return 'cr:extension';
      case ReportingType.USER:
        return 'management:account-circle';
      case ReportingType.USER_ACTIVITY:
        return 'management:public';
      default:
        return 'cr:security';
    }
  },

  /**
   * Handles the 'search-changed' event fired from the toolbar.
   * Redirects to the settings page initialized the the current
   * search query.
   * @param {!CustomEvent<string>} e
   * @private
   */
  onSearchChanged_(e) {
    const query = e.detail;
    window.location.href =
        `chrome://settings?search=${encodeURIComponent(query)}`;
  },

  /** @private */
  onTapBack_() {
    if (history.length > 1) {
      history.back();
    } else {
      window.location.href = 'chrome://settings/help';
    }
  },

  /** @private */
  updateManagedFields_() {
    this.browserProxy_.getContextualManagedData().then(data => {
      this.managed_ = data.managed;
      this.extensionReportingSubtitle_ = data.extensionReportingTitle;
      this.managedWebsitesSubtitle_ = data.managedWebsitesSubtitle;
      this.subtitle_ = data.pageSubtitle;
      // <if expr="chromeos">
      this.customerLogo_ = data.customerLogo;
      this.managementOverview_ = data.overview;
      this.eolMessage_ = data.eolMessage;
      this.showProxyServerPrivacyDisclosure_ =
          data.showProxyServerPrivacyDisclosure;
      try {
        // Sanitizing the message could throw an error if it contains non
        // supported markup.
        this.eolAdminMessage_ =
            loadTimeData.sanitizeInnerHtml(data.eolAdminMessage);
      } catch (e) {
        this.eolAdminMessage_ = '';
      }
      // </if>
      // <if expr="not chromeos">
      this.managementNoticeHtml_ = data.browserManagementNotice;
      // </if>
    });
  },
});
