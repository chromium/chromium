// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './icons.js';
import './strings.m.js';

import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserReportingResponse, Extension, ManagementBrowserProxy, ManagementBrowserProxyImpl, ReportingType, ThreatProtectionInfo} from './management_browser_proxy.js';
// <if expr="chromeos_ash">
import {DeviceReportingResponse, DeviceReportingType} from './management_browser_proxy.js';
// </if>

type BrowserReportingData = {
  messageIds: string[],
  icon: string,
};

const ManagementUiElementBase = WebUIListenerMixin(I18nMixin(PolymerElement));

class ManagementUiElement extends ManagementUiElementBase {
  static get is() {
    return 'management-ui';
  }

  static get properties() {
    return {
      /**
       * List of messages related to browser reporting.
       */
      browserReportingInfo_: Array,

      /**
       * List of messages related to browser reporting.
       */
      extensions_: Array,

      /**
       * List of messages related to browser reporting.
       */
      managedWebsites_: Array,

      managedWebsitesSubtitle_: String,

      // <if expr="chromeos_ash">
      /**
       * List of messages related to device reporting.
       */
      deviceReportingInfo_: Array,

      /**
       * Message stating if the Trust Roots are configured.
       */
      localTrustRoots_: String,

      customerLogo_: String,
      managementOverview_: String,
      pluginVmDataCollectionEnabled_: Boolean,
      eolAdminMessage_: String,
      eolMessage_: String,
      showProxyServerPrivacyDisclosure_: Boolean,
      // </if>

      subtitle_: String,

      // <if expr="not chromeos_ash">
      managementNoticeHtml_: String,
      // </if>

      managed_: Boolean,
      extensionReportingSubtitle_: String,
      threatProtectionInfo_: Object,
    };
  }

  private browserReportingInfo_: Array<BrowserReportingData>|null;
  private extensions_: Array<Extension>|null;
  private managedWebsites_: string[]|null;
  private managedWebsitesSubtitle_: string;

  // <if expr="chromeos_ash">
  private deviceReportingInfo_: Array<DeviceReportingResponse>|null;
  private localTrustRoots_: string;
  private customerLogo_: string;
  private managementOverview_: string;
  private pluginVmDataCollectionEnabled_: boolean;
  private eolAdminMessage_: string;
  private eolMessage_: string;
  private showProxyServerPrivacyDisclosure_: boolean;
  // </if>

  private subtitle_: string;

  // <if expr="not chromeos_ash">
  private managementNoticeHtml_: string;
  // </if>

  private managed_: boolean;
  private extensionReportingSubtitle_: string;
  private threatProtectionInfo_: ThreatProtectionInfo;
  private browserProxy_: ManagementBrowserProxy|null = null;

  /** @override */
  override connectedCallback() {
    super.connectedCallback();

    document.documentElement.classList.remove('loading');
    this.browserProxy_ = ManagementBrowserProxyImpl.getInstance();
    this.updateManagedFields_();
    this.initBrowserReportingInfo_();
    this.getThreatProtectionInfo_();

    this.addWebUIListener(
        'browser-reporting-info-updated',
        (reportingInfo: Array<BrowserReportingResponse>) =>
            this.onBrowserReportingInfoReceived_(reportingInfo));

    // <if expr="chromeos_ash">
    this.addWebUIListener(
        'plugin-vm-data-collection-updated',
        (enabled: boolean) => this.pluginVmDataCollectionEnabled_ = enabled);
    // </if>

    this.addWebUIListener('managed_data_changed', () => {
      this.updateManagedFields_();
    });

    this.addWebUIListener(
        'threat-protection-info-updated',
        (info: ThreatProtectionInfo) => this.threatProtectionInfo_ = info);

    this.getExtensions_();
    this.getManagedWebsites_();
    // <if expr="chromeos_ash">
    this.getDeviceReportingInfo_();
    this.getPluginVmDataCollectionStatus_();
    this.getLocalTrustRootsInfo_();
    // </if>
  }

  private initBrowserReportingInfo_() {
    this.browserProxy_!.initBrowserReportingInfo().then(
        reportingInfo => this.onBrowserReportingInfoReceived_(reportingInfo));
  }

  private onBrowserReportingInfoReceived_(reportingInfo:
                                              Array<BrowserReportingResponse>) {
    const reportingInfoMap = reportingInfo.reduce((info, response) => {
      info[response.reportingType] = info[response.reportingType] || {
        icon: this.getIconForReportingType_(response.reportingType),
        messageIds: []
      };
      info[response.reportingType].messageIds.push(response.messageId);
      return info;
    }, {} as {[k: string]: {icon: string, messageIds: string[]}});

    const reportingTypeOrder = {
      [ReportingType.SECURITY]: 1,
      [ReportingType.EXTENSIONS]: 2,
      [ReportingType.USER]: 3,
      [ReportingType.USER_ACTIVITY]: 4,
      [ReportingType.DEVICE]: 5,
    } as {[k: string]: number};

    this.browserReportingInfo_ =
        Object.keys(reportingInfoMap)
            .sort((a, b) => reportingTypeOrder[a] - reportingTypeOrder[b])
            .map(reportingType => reportingInfoMap[reportingType]);
  }

  private getExtensions_() {
    this.browserProxy_!.getExtensions().then(extensions => {
      this.extensions_ = extensions;
    });
  }

  private getManagedWebsites_() {
    this.browserProxy_!.getManagedWebsites().then(managedWebsites => {
      this.managedWebsites_ = managedWebsites;
    });
  }

  private getThreatProtectionInfo_() {
    this.browserProxy_!.getThreatProtectionInfo().then(info => {
      this.threatProtectionInfo_ = info;
    });
  }

  /**
   * @return Whether there is threat protection info to show.
   */
  private showThreatProtectionInfo_(): boolean {
    return !!this.threatProtectionInfo_ &&
        this.threatProtectionInfo_.info.length > 0;
  }

  // <if expr="chromeos_ash">
  private getLocalTrustRootsInfo_() {
    this.browserProxy_!.getLocalTrustRootsInfo().then(trustRootsConfigured => {
      this.localTrustRoots_ = trustRootsConfigured ?
          loadTimeData.getString('managementTrustRootsConfigured') :
          '';
    });
  }

  private getDeviceReportingInfo_() {
    this.browserProxy_!.getDeviceReportingInfo().then(reportingInfo => {
      this.deviceReportingInfo_ = reportingInfo;
    });
  }

  private getPluginVmDataCollectionStatus_() {
    this.browserProxy_!.getPluginVmDataCollectionStatus().then(
        pluginVmDataCollectionEnabled => {
          this.pluginVmDataCollectionEnabled_ = pluginVmDataCollectionEnabled;
        });
  }

  /**
   * @return Whether there are device reporting info to show.
   */
  private showDeviceReportingInfo_(): boolean {
    return !!this.deviceReportingInfo_ && this.deviceReportingInfo_.length > 0;
  }

  /**
   * @param eolAdminMessage The device return instructions
   * @return Whether there are device return instructions from the
   *     admin in case an update is required after reaching end of life.
   */
  private isEmpty_(eolAdminMessage: string): boolean {
    return !eolAdminMessage || eolAdminMessage.trim().length === 0;
  }

  /**
   * @return The associated icon.
   */
  private getIconForDeviceReportingType_(reportingType: DeviceReportingType):
      string {
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
      case DeviceReportingType.DLP_EVENTS:
        return 'management:policy';
      case DeviceReportingType.CROSTINI:
        return 'management:linux';
      case DeviceReportingType.USERNAME:
        return 'management:account-circle';
      case DeviceReportingType.EXTENSION:
        return 'cr:extension';
      case DeviceReportingType.ANDROID_APPLICATION:
        return 'management:play-store';
      case DeviceReportingType.LOGIN_LOGOUT:
        return 'management:timelapse';
      case DeviceReportingType.CRD_SESSIONS:
        return 'management:timelapse';
      case DeviceReportingType.PERIPHERALS:
        return 'management:usb';
      default:
        return 'cr:computer';
    }
  }
  // </if>

  /**
   * @return Whether there are browser reporting info to show.
   */
  private showBrowserReportingInfo_(): boolean {
    return !!this.browserReportingInfo_ &&
        this.browserReportingInfo_.length > 0;
  }

  /**
   * @return Whether there are extension reporting info to show.
   */
  private showExtensionReportingInfo_(): boolean {
    return !!this.extensions_ && this.extensions_.length > 0;
  }

  /**
   * @return Whether there is managed websites info to show.
   */
  private showManagedWebsitesInfo_(): boolean {
    return !!this.managedWebsites_ && this.managedWebsites_.length > 0;
  }


  /**
   * @return The associated icon.
   */
  private getIconForReportingType_(reportingType: ReportingType): string {
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
  }

  /**
   * Handles the 'search-changed' event fired from the toolbar.
   * Redirects to the settings page initialized the the current
   * search query.
   */
  private onSearchChanged_(e: CustomEvent<string>) {
    const query = e.detail;
    window.location.href =
        `chrome://settings?search=${encodeURIComponent(query)}`;
  }

  private onTapBack_() {
    if (history.length > 1) {
      history.back();
    } else {
      window.location.href = 'chrome://settings/help';
    }
  }

  private updateManagedFields_() {
    this.browserProxy_!.getContextualManagedData().then(data => {
      this.managed_ = data.managed;
      this.extensionReportingSubtitle_ = data.extensionReportingTitle;
      this.managedWebsitesSubtitle_ = data.managedWebsitesSubtitle;
      this.subtitle_ = data.pageSubtitle;
      // <if expr="chromeos_ash">
      this.customerLogo_ = data.customerLogo;
      this.managementOverview_ = data.overview;
      this.eolMessage_ = data.eolMessage;
      this.showProxyServerPrivacyDisclosure_ =
          data.showProxyServerPrivacyDisclosure;
      try {
        // Sanitizing the message could throw an error if it contains non
        // supported markup.
        this.eolAdminMessage_ = sanitizeInnerHtml(data.eolAdminMessage);
      } catch (e) {
        this.eolAdminMessage_ = '';
      }
      // </if>
      // <if expr="not chromeos_ash">
      this.managementNoticeHtml_ = data.browserManagementNotice;
      // </if>
    });
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(ManagementUiElement.is, ManagementUiElement);
