// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

export type Extension = {
  name: string,
  permissions: string[],
};

export enum ReportingType {
  SECURITY = 'security',
  DEVICE = 'device',
  USER = 'user',
  USER_ACTIVITY = 'user-activity',
  EXTENSIONS = 'extensions',
}

export type BrowserReportingResponse = {
  messageId: string,
  reportingType: ReportingType,
};

type ManagedDataResponse = {
  browserManagementNotice: string,
  extensionReportingTitle: string,
  managedWebsitesSubtitle: string,
  pageSubtitle: string,
  managed: boolean,
  overview: string,
  customerLogo: string,
  threatProtectionDescription: string,
  showUpdateRequiredEol: boolean,
  eolMessage: string,
  eolAdminMessage: string,
  showProxyServerPrivacyDisclosure: boolean,
};

type ThreatProtectionPermission = {
  title: string,
  permission: string,
};

export type ThreatProtectionInfo = {
  info: ThreatProtectionPermission[],
  description: string,
};

// <if expr="chromeos_ash">
/**
 * @enum {string} Look at ToJSDeviceReportingType usage in
 *    management_ui_handler.cc for more details.
 */
export enum DeviceReportingType {
  SUPERVISED_USER = 'supervised user',
  DEVICE_ACTIVITY = 'device activity',
  STATISTIC = 'device statistics',
  DEVICE = 'device',
  CRASH_REPORT = 'crash report',
  APP_INFO_AND_ACTIVITY = 'app info and activity',
  LOGS = 'logs',
  PRINT = 'print',
  PRINT_JOBS = 'print jobs',
  DLP_EVENTS = 'dlp events',
  CROSTINI = 'crostini',
  USERNAME = 'username',
  EXTENSION = 'extension',
  ANDROID_APPLICATION = 'android application',
  LOGIN_LOGOUT = 'login-logout',
  CRD_SESSIONS = 'crd sessions',
  PERIPHERALS = 'peripherals',
}


export type DeviceReportingResponse = {
  messageId: string,
  reportingType: DeviceReportingType,
};
// </if>

/** @interface */
export interface ManagementBrowserProxy {
  getExtensions(): Promise<Extension[]>;

  getManagedWebsites(): Promise<string[]>;

  // <if expr="chromeos_ash">
  /**
   * @return Whether trust root configured or not.
   */
  getLocalTrustRootsInfo(): Promise<boolean>;

  /**
   * @return List of items to display in device reporting section.
   */
  getDeviceReportingInfo(): Promise<DeviceReportingResponse[]>;

  /**
   * @return Whether the Plugin VM data collection is enabled or not.
   */
  getPluginVmDataCollectionStatus(): Promise<boolean>;
  // </if>

  getContextualManagedData(): Promise<ManagedDataResponse>;

  getThreatProtectionInfo(): Promise<ThreatProtectionInfo>;

  /**
   * @return The list of browser reporting info messages.
   */
  initBrowserReportingInfo(): Promise<BrowserReportingResponse[]>;
}

export class ManagementBrowserProxyImpl implements ManagementBrowserProxy {
  getExtensions() {
    return sendWithPromise('getExtensions');
  }

  getManagedWebsites() {
    return sendWithPromise('getManagedWebsites');
  }

  // <if expr="chromeos_ash">
  getLocalTrustRootsInfo() {
    return sendWithPromise('getLocalTrustRootsInfo');
  }

  getDeviceReportingInfo() {
    return sendWithPromise('getDeviceReportingInfo');
  }

  getPluginVmDataCollectionStatus() {
    return sendWithPromise('getPluginVmDataCollectionStatus');
  }
  // </if>

  getContextualManagedData() {
    return sendWithPromise('getContextualManagedData');
  }

  getThreatProtectionInfo() {
    return sendWithPromise('getThreatProtectionInfo');
  }

  initBrowserReportingInfo() {
    return sendWithPromise('initBrowserReportingInfo');
  }

  static getInstance(): ManagementBrowserProxy {
    return instance || (instance = new ManagementBrowserProxyImpl());
  }
}

let instance: ManagementBrowserProxy|null = null;

declare global {
  interface Window {
    ManagementBrowserProxyImpl: typeof ManagementBrowserProxyImpl;
  }
}

// Export |ManagementBrowserProxyImpl| on |window| so that it can be accessed by
// management_ui_browsertest.cc
window.ManagementBrowserProxyImpl = ManagementBrowserProxyImpl;
