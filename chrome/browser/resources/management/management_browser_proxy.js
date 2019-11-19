// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * @typedef {{
 *   name: string,
 *   permissions: !Array<string>
 * }}
 */
export let Extension;

/** @enum {string} */
export const ReportingType = {
  SECURITY: 'security',
  DEVICE: 'device',
  USER: 'user',
  USER_ACTIVITY: 'user-activity',
  EXTENSIONS: 'extensions'
};

/**
 * @typedef {{
 *   messageId: string,
 *   reportingType: !ReportingType,
 * }}
 */
export let BrowserReportingResponse;

/**
 * @typedef {{
 *   browserManagementNotice: string,
 *   extensionReportingTitle: string,
 *   pageSubtitle: string,
 *   managed: boolean,
 *   overview: string,
 *   customerLogo: string,
 *   threatProtectionDescription: string
 * }}
 */
let ManagedDataResponse;

/**
 * @typedef {{
 *  title: string,
 *  permission: string
 * }}
 */
let ThreatProtectionPermission;

/**
 * @typedef {{
 *   info: !Array<!ThreatProtectionPermission>,
 *   description: string
 * }}
 */
export let ThreatProtectionInfo;

// <if expr="chromeos">
/**
 * @enum {string} Look at ToJSDeviceReportingType usage in
 *    management_ui_handler.cc for more details.
 */
export const DeviceReportingType = {
  SUPERVISED_USER: 'supervised user',
  DEVICE_ACTIVITY: 'device activity',
  STATISTIC: 'device statistics',
  DEVICE: 'device',
  LOGS: 'logs',
  PRINT: 'print',
  CROSTINI: 'crostini'
};


/**
 * @typedef {{
 *   messageId: string,
 *   reportingType: !DeviceReportingType,
 * }}
 */
export let DeviceReportingResponse;
// </if>

/** @interface */
export class ManagementBrowserProxy {
  /** @return {!Promise<!Array<!Extension>>} */
  getExtensions() {}

  // <if expr="chromeos">
  /**
   * @return {!Promise<boolean>} Boolean describing trust root configured
   *     or not.
   */
  getLocalTrustRootsInfo() {}

  /**
   * @return {!Promise<!Array<DeviceReportingResponse>>} List of
   *     items to display in device reporting section.
   */
  getDeviceReportingInfo() {}
  // </if>

  /** @return {!Promise<!ManagedDataResponse>} */
  getContextualManagedData() {}

  /** @return {!Promise<!ThreatProtectionInfo>} */
  getThreatProtectionInfo() {}

  /**
   * @return {!Promise<!Array<!BrowserReportingResponse>>} The list
   *     of browser reporting info messages.
   */
  initBrowserReportingInfo() {}
}

/** @implements {ManagementBrowserProxy} */
export class ManagementBrowserProxyImpl {
  /** @override */
  getExtensions() {
    return sendWithPromise('getExtensions');
  }

  // <if expr="chromeos">
  /** @override */
  getLocalTrustRootsInfo() {
    return sendWithPromise('getLocalTrustRootsInfo');
  }

  /** @override */
  getDeviceReportingInfo() {
    return sendWithPromise('getDeviceReportingInfo');
  }
  // </if>

  /** @override */
  getContextualManagedData() {
    return sendWithPromise('getContextualManagedData');
  }

  /** @override */
  getThreatProtectionInfo() {
    return sendWithPromise('getThreatProtectionInfo');
  }

  /** @override */
  initBrowserReportingInfo() {
    return sendWithPromise('initBrowserReportingInfo');
  }
}

addSingletonGetter(ManagementBrowserProxyImpl);

// Export |ManagementBrowserProxyImpl| on |window| so that it can be accessed by
// management_ui_browsertest.cc
window.ManagementBrowserProxyImpl = ManagementBrowserProxyImpl;
