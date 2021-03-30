// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles interprocess communication for the privacy page. */

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

  /** @typedef {{enabled: boolean, managed: boolean}} */
export let MetricsReporting;

/** @typedef {{name: string, value: string, policy: string}} */
export let ResolverOption;

/**
 * Contains the possible string values for the secure DNS mode. This must be
 * kept in sync with the mode names in chrome/browser/net/secure_dns_config.h.
 * @enum {string}
 */
export const SecureDnsMode = {
  OFF: 'off',
  AUTOMATIC: 'automatic',
  SECURE: 'secure',
};

/**
 * Contains the possible management modes. This should be kept in sync with
 * the management modes in chrome/browser/net/secure_dns_config.h.
 * @enum {number}
 */
export const SecureDnsUiManagementMode = {
  NO_OVERRIDE: 0,
  DISABLED_MANAGED: 1,
  DISABLED_PARENTAL_CONTROLS: 2,
};

/**
 * @typedef {{
 *   mode: SecureDnsMode,
 *   templates: !Array<string>,
 *   managementMode: SecureDnsUiManagementMode
 * }}
 */
export let SecureDnsSetting;

/** @interface */
export class PrivacyPageBrowserProxy {
  // <if expr="_google_chrome and not chromeos">
  /** @return {!Promise<!MetricsReporting>} */
  getMetricsReporting() {}

  /** @param {boolean} enabled */
  setMetricsReportingEnabled(enabled) {}

  // </if>

  // <if expr="is_win or is_macosx">
  /** Invokes the native certificate manager (used by win and mac). */
  showManageSSLCertificates() {}

  // </if>

  /** @param {boolean} enabled */
  setBlockAutoplayEnabled(enabled) {}

  /** @return {!Promise<!Array<!ResolverOption>>} */
  getSecureDnsResolverList() {}

  /** @return {!Promise<!SecureDnsSetting>} */
  getSecureDnsSetting() {}

  /**
   * Returns the URL templates, if they are all valid.
   * @param {string} entry
   * @return {!Promise<!Array<string>>}
   */
  parseCustomDnsEntry(entry) {}

  /**
   * Returns True if a test query to the secure DNS template succeeded
   * or was cancelled.
   * @param {string} template
   * @return {!Promise<boolean>}
   */
  probeCustomDnsTemplate(template) {}

  /**
   * Records metrics on the user's interaction with the dropdown menu.
   * @param {string} oldSelection value of previously selected dropdown option
   * @param {string} newSelection value of newly selected dropdown option
   */
  recordUserDropdownInteraction(oldSelection, newSelection) {}
}

/**
 * @implements {PrivacyPageBrowserProxy}
 */
export class PrivacyPageBrowserProxyImpl {
  // <if expr="_google_chrome and not chromeos">
  /** @override */
  getMetricsReporting() {
    return sendWithPromise('getMetricsReporting');
  }

  /** @override */
  setMetricsReportingEnabled(enabled) {
    chrome.send('setMetricsReportingEnabled', [enabled]);
  }

  // </if>

  /** @override */
  setBlockAutoplayEnabled(enabled) {
    chrome.send('setBlockAutoplayEnabled', [enabled]);
  }

  // <if expr="is_win or is_macosx">
  /** @override */
  showManageSSLCertificates() {
    chrome.send('showManageSSLCertificates');
  }
  // </if>

  /** @override */
  getSecureDnsResolverList() {
    return sendWithPromise('getSecureDnsResolverList');
  }

  /** @override */
  getSecureDnsSetting() {
    return sendWithPromise('getSecureDnsSetting');
  }

  /** @override */
  parseCustomDnsEntry(entry) {
    return sendWithPromise('parseCustomDnsEntry', entry);
  }

  /** @override */
  probeCustomDnsTemplate(template) {
    return sendWithPromise('probeCustomDnsTemplate', template);
  }

  /** override */
  recordUserDropdownInteraction(oldSelection, newSelection) {
    chrome.send('recordUserDropdownInteraction', [oldSelection, newSelection]);
  }
}

addSingletonGetter(PrivacyPageBrowserProxyImpl);
