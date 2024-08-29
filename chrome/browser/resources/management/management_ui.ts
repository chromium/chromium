// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './icons.html.js';
import './strings.m.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
// <if expr="is_chromeos">
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
// </if>
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

// clang-format off
import type {Application, BrowserReportingResponse, Extension, ManagementBrowserProxy, ThreatProtectionInfo} from './management_browser_proxy.js';
import { ManagementBrowserProxyImpl, ReportingType} from './management_browser_proxy.js';
// <if expr="is_chromeos">
import type {DeviceReportingResponse} from './management_browser_proxy.js';
import { DeviceReportingType} from './management_browser_proxy.js';
// </if>
import {getCss} from './management_ui.css.js';
import {getHtml} from './management_ui.html.js';
// clang-format on

interface BrowserReportingData {
  messageIds: string[];
  icon?: string;
}

const ManagementUiElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class ManagementUiElement extends ManagementUiElementBase {
  static get is() {
    return 'management-ui';
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
       * List of messages related to application reporting.
       */
      applications_: {type: Array},

      /**
       * Title of subsection for application reporting.
       */
      applicationReportingSubtitle_: {type: String},

      /**
       * List of messages related to browser reporting.
       */
      browserReportingInfo_: {type: Array},

      /**
       * List of messages related to profile reporting.
       */
      profileReportingInfo_: {type: Array},

      /**
       * List of messages related to extension reporting.
       */
      extensions_: {type: Array},

      /**
       * Title of subsection for extension reporting.
       */
      extensionReportingSubtitle_: {type: String},

      /**
       * List of messages related to managed websites reporting.
       */
      managedWebsites_: {type: Array},

      managedWebsitesSubtitle_: {type: String},

      // <if expr="is_chromeos">
      /**
       * List of messages related to device reporting.
       */
      deviceReportingInfo_: {type: Array},

      /**
       * Message stating if the Trust Roots are configured.
       */
      localTrustRoots_: {type: String},

      /**
       * Message stating if uploading of downloads or screenshots to cloud
       * storage is configured.
       */
      filesUploadToCloud_: {type: String},

      customerLogo_: {type: String},
      managementOverview_: {type: String},
      pluginVmDataCollectionEnabled_: {type: Boolean},
      eolAdminMessage_: {type: String},
      eolMessage_: {type: String},
      showMonitoredNetworkPrivacyDisclosure_: {type: Boolean},
      // </if>

      subtitle_: {type: String},

      // <if expr="not chromeos_ash">
      managementNoticeHtml_: {type: String},
      // </if>

      managed_: {type: Boolean},
      threatProtectionInfo_: {type: Object},
    };
  }

  protected applications_: Application[]|null = null;
  protected browserReportingInfo_: BrowserReportingData[]|null = null;
  protected profileReportingInfo_: BrowserReportingData[]|null = null;
  protected extensions_: Extension[]|null = null;
  protected managedWebsites_: string[]|null = null;
  protected managedWebsitesSubtitle_: string = '';

  // <if expr="is_chromeos">
  protected deviceReportingInfo_: DeviceReportingResponse[]|null = null;
  protected localTrustRoots_: string = '';
  protected filesUploadToCloud_: string = '';
  protected customerLogo_: string = '';
  protected managementOverview_: string = '';
  protected pluginVmDataCollectionEnabled_: boolean = false;
  protected eolAdminMessage_: string = '';
  protected eolMessage_: string = '';
  protected showMonitoredNetworkPrivacyDisclosure_: boolean = false;
  // </if>

  protected subtitle_: string = '';

  // <if expr="not chromeos_ash">
  protected managementNoticeHtml_: TrustedHTML = window.trustedTypes!.emptyHTML;
  // </if>

  protected managed_: boolean = false;
  protected applicationReportingSubtitle_: string = '';
  protected extensionReportingSubtitle_: string = '';
  protected threatProtectionInfo_: ThreatProtectionInfo|null = null;
  private browserProxy_: ManagementBrowserProxy =
      ManagementBrowserProxyImpl.getInstance();

  /** @override */
  override connectedCallback() {
    super.connectedCallback();

    document.documentElement.classList.remove('loading');
    this.updateManagedFields_();
    this.initReportingInfo_();
    this.getThreatProtectionInfo_();

    this.addWebUiListener(
        'browser-reporting-info-updated',
        (reportingInfo: BrowserReportingResponse[]) =>
            this.onBrowserReportingInfoReceived_(reportingInfo));

    this.addWebUiListener(
        'profile-reporting-info-updated',
        (reportingInfo: BrowserReportingResponse[]) =>
            this.onProfileReportingInfoReceived_(reportingInfo));

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
    this.getFilesUploadToCloudInfo_();
    // </if>
  }

  private initReportingInfo_() {
    this.browserProxy_.initBrowserReportingInfo().then(
        reportingInfo => this.onBrowserReportingInfoReceived_(reportingInfo));
    this.browserProxy_.initProfileReportingInfo().then(
        reportingInfo => this.onProfileReportingInfoReceived_(reportingInfo));
  }

  private onBrowserReportingInfoReceived_(reportingInfo:
                                              BrowserReportingResponse[]) {
    const reportingInfoMap = reportingInfo.reduce((info, response) => {
      info[response.reportingType] = info[response.reportingType] || {
        icon: this.getIconForReportingType_(response.reportingType),
        messageIds: [],
      };
      info[response.reportingType]!.messageIds.push(response.messageId);
      return info;
    }, {} as {[k: string]: {icon: string, messageIds: string[]}});

    const reportingTypeOrder: {[k: string]: number} = {
      [ReportingType.URL]: 1,
      [ReportingType.SECURITY]: 2,
      [ReportingType.EXTENSIONS]: 3,
      [ReportingType.USER]: 4,
      [ReportingType.USER_ACTIVITY]: 5,
      [ReportingType.DEVICE]: 6,
      [ReportingType.LEGACY_TECH]: 7,
    };

    this.browserReportingInfo_ =
        Object.keys(reportingInfoMap)
            .sort((a, b) => reportingTypeOrder[a]! - reportingTypeOrder[b]!)
            .map(reportingType => reportingInfoMap[reportingType]) as
        BrowserReportingData[];
  }


  private onProfileReportingInfoReceived_(reportingInfo:
                                              BrowserReportingResponse[]) {
    this.profileReportingInfo_ =
        reportingInfo.map((info) => ({
                            messageIds: [info.messageId],
                          }));
  }
  private getExtensions_() {
    this.browserProxy_.getExtensions().then(extensions => {
      this.extensions_ = extensions;
    });
  }

  private getManagedWebsites_() {
    this.browserProxy_.getManagedWebsites().then(managedWebsites => {
      this.managedWebsites_ = managedWebsites;
    });
  }

  private getApplications_() {
    this.browserProxy_.getApplications().then(applications => {
      this.applications_ = applications;
    });
  }

  private getThreatProtectionInfo_() {
    this.browserProxy_.getThreatProtectionInfo().then(info => {
      this.threatProtectionInfo_ = info;
    });
  }

  /**
   * @return Whether there is threat protection info to show.
   */
  protected showThreatProtectionInfo_(): boolean {
    return !!this.threatProtectionInfo_ &&
        this.threatProtectionInfo_.info.length > 0;
  }

  // <if expr="is_chromeos">
  private getLocalTrustRootsInfo_() {
    this.browserProxy_.getLocalTrustRootsInfo().then(trustRootsConfigured => {
      this.localTrustRoots_ = trustRootsConfigured ?
          loadTimeData.getString('managementTrustRootsConfigured') :
          '';
    });
  }

  private getFilesUploadToCloudInfo_() {
    this.browserProxy_.getFilesUploadToCloudInfo().then(info => {
      this.filesUploadToCloud_ = info;
    });
  }

  private getDeviceReportingInfo_() {
    this.browserProxy_.getDeviceReportingInfo().then(reportingInfo => {
      this.deviceReportingInfo_ = reportingInfo;
    });
  }

  private getPluginVmDataCollectionStatus_() {
    this.browserProxy_.getPluginVmDataCollectionStatus().then(
        pluginVmDataCollectionEnabled => {
          this.pluginVmDataCollectionEnabled_ = pluginVmDataCollectionEnabled;
        });
  }

  /**
   * @return Whether there are device reporting info to show.
   */
  protected showDeviceReportingInfo_(): boolean {
    return !!this.deviceReportingInfo_ && this.deviceReportingInfo_.length > 0;
  }

  /**
   * @param eolAdminMessage The device return instructions
   * @return Whether there are device return instructions from the
   *     admin in case an update is required after reaching end of life.
   */
  protected isEolAdminMessageEmpty_(): boolean {
    return !this.eolAdminMessage_ || this.eolAdminMessage_.trim().length === 0;
  }

  /**
   * @return The associated icon.
   */
  protected getIconForDeviceReportingType_(reportingType: DeviceReportingType):
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
      case DeviceReportingType.LEGACY_TECH:
        return 'management:legacy-tech';
      case DeviceReportingType.WEBSITE_INFO_AND_ACTIVITY:
        return 'management:web';
      case DeviceReportingType.FILE_EVENTS:
        return 'management:policy';
      default:
        return 'cr:computer';
    }
  }

  protected getDeviceReportingHtmlContent_(response: DeviceReportingResponse):
      TrustedHTML {
    return this.i18nAdvanced(
        response.messageId, {substitutions: response.messageParams});
  }
  // </if>

  /**
   * @return Whether there are browser reporting info to show.
   */
  protected showBrowserReportingInfo_(): boolean {
    return !!this.browserReportingInfo_ &&
        this.browserReportingInfo_.length > 0;
  }

  /**
   * @return Whether there are profile reporting info to show with new format.
   */
  protected showProfileReportingInfo_(): boolean {
    return !!this.profileReportingInfo_ &&
        this.profileReportingInfo_.length > 0;
  }


  /**
   * @return Whether there are extension reporting info to show.
   */
  protected showExtensionReportingInfo_(): boolean {
    return !!this.extensions_ && this.extensions_.length > 0;
  }

  /**
   * @return Whether there are application reporting info to show.
   */
  protected showApplicationReportingInfo_(): boolean {
    return !!this.applications_ && this.applications_.length > 0;
  }

  /**
   * @return Whether there is managed websites info to show.
   */
  protected showManagedWebsitesInfo_(): boolean {
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
      case ReportingType.LEGACY_TECH:
        return 'management:legacy-tech';
      case ReportingType.URL:
        return 'management:link';
      default:
        return 'cr:security';
    }
  }

  /**
   * Handles the 'search-changed' event fired from the toolbar.
   * Redirects to the settings page initialized the the current
   * search query.
   */
  protected onSearchChanged_(e: CustomEvent<string>) {
    const query = e.detail;
    window.location.href =
        `chrome://settings?search=${encodeURIComponent(query)}`;
  }

  protected onTapBack_() {
    if (history.length > 1) {
      history.back();
    } else {
      window.location.href = 'chrome://settings/help';
    }
  }

  private updateManagedFields_() {
    this.browserProxy_.getContextualManagedData().then(data => {
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
