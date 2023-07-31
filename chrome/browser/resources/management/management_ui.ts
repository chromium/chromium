// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './icons.html.js';
import './strings.m.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// clang-format off
import {Application, BrowserReportingResponse, Extension, ManagementBrowserProxy, ManagementBrowserProxyImpl, ReportingType, ThreatProtectionInfo} from './management_browser_proxy.js';
// <if expr="is_chromeos">
import {DeviceReportingResponse, DeviceReportingType} from './management_browser_proxy.js';
// </if>
import {getTemplate} from './management_ui.html.js';
// clang-format on

interface BrowserReportingData {
  messageIds: string[];
  icon: string;
}

const ManagementUiElementBase = WebUiListenerMixin(I18nMixin(PolymerElement));

class ManagementUiElement extends ManagementUiElementBase {
  static get is() {
    return 'management-ui';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * List of messages related to application reporting.
       */
      applications_: Array,

      /**
       * Title of subsection for application reporting.
       */
      applicationReportingSubtitle_: String,

      /**
       * List of messages related to browser reporting.
       */
      browserReportingInfo_: Array,

      /**
       * List of messages related to extension reporting.
       */
      extensions_: Array,

      /**
       * Title of subsection for extension reporting.
       */
      extensionReportingSubtitle_: String,

      /**
       * List of messages related to managed websites reporting.
       */
      managedWebsites_: Array,

      managedWebsitesSubtitle_: String,

      // <if expr="is_chromeos">
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
      showMonitoredNetworkPrivacyDisclosure_: Boolean,
      // </if>

      subtitle_: String,

      // <if expr="not chromeos_ash">
      managementNoticeHtml_: String,
      // </if>

      managed_: Boolean,
      threatProtectionInfo_: Object,
    };
  }

  private applications_: Application[]|null;
  private browserReportingInfo_: BrowserReportingData[]|null;
  private extensions_: Extension[]|null;
  private managedWebsites_: string[]|null;
  private managedWebsitesSubtitle_: string;

  // <if expr="is_chromeos">
  private deviceReportingInfo_: DeviceReportingResponse[]|null;
  private localTrustRoots_: string;
  private customerLogo_: string;
  private managementOverview_: string;
  private pluginVmDataCollectionEnabled_: boolean;
  private eolAdminMessage_: string;
  private eolMessage_: string;
  private showMonitoredNetworkPrivacyDisclosure_: boolean;
  // </if>

  private subtitle_: string;

  // <if expr="not chromeos_ash">
  private managementNoticeHtml_: TrustedHTML;
  // </if>

  private managed_: boolean;
  private applicationReportingSubtitle_: string;
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

    this.addWebUiListener(
        'browser-reporting-info-updated',
        (reportingInfo: BrowserReportingResponse[]) =>
            this.onBrowserReportingInfoReceived_(reportingInfo));

    // <if expr="is_chromeos">
    this.addWebUiListener(
        'plugin-vm-data-collection-updated',
        (enabled: boolean) => this.pluginVmDataCollectionEnabled_ = enabled);
    // </if>

    this.addWebUiListener('managed_data_changed', () => {
      this.updateManagedFields_();
    });

    this.addWebUiListener(
        'threat-protection-info-updated',
        (info: ThreatProtectionInfo) => this.threatProtectionInfo_ = info);

    this.getExtensions_();
    this.getManagedWebsites_();
    this.getApplications_();
    // <if expr="is_chromeos">
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
                                              BrowserReportingResponse[]) {
    const reportingInfoMap = reportingInfo.reduce((info, response) => {
      info[response.reportingType] = info[response.reportingType] || {
        icon: this.getIconForReportingType_(response.reportingType),
        messageIds: [],
      };
      info[response.reportingType].messageIds.push(response.messageId);
      return info;
    }, {} as {[k: string]: {icon: string, messageIds: string[]}});

    const reportingTypeOrder: {[k: string]: number} = {
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

  private getApplications_() {
    this.browserProxy_!.getApplications().then(applications => {
      this.applications_ = applications;
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

  // <if expr="is_chromeos">
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
   * @return Whether there are application reporting info to show.
   */
  private showApplicationReportingInfo_(): boolean {
    return !!this.applications_ && this.applications_.length > 0;
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
      this.extensionReportingSubtitle_ = data.extensionReportingSubtitle;
      this.managedWebsitesSubtitle_ = data.managedWebsitesSubtitle;
      this.applicationReportingSubtitle_ = data.applicationReportingSubtitle;
      this.subtitle_ = data.pageSubtitle;
      // <if expr="chromeos_ash">
      this.customerLogo_ = data.customerLogo;
      this.managementOverview_ = data.overview;
      this.eolMessage_ = data.eolMessage;
      this.showMonitoredNetworkPrivacyDisclosure_ =
          data.showMonitoredNetworkPrivacyDisclosure;
      try {
        // Sanitizing the message could throw an error if it contains non
        // supported markup.
        sanitizeInnerHtml(data.eolAdminMessage);
        this.eolAdminMessage_ = data.eolAdminMessage;
      } catch (e) {
        this.eolAdminMessage_ = '';
      }
      // </if>
      // <if expr="not chromeos_ash">
      this.managementNoticeHtml_ = sanitizeInnerHtml(
          data.browserManagementNotice, {attrs: ['aria-label']});
      // </if>
    });
  }
}

customElements.define(ManagementUiElement.is, ManagementUiElement);
