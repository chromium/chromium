// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles interprocess communication for the privacy page. */

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

export interface MetricsReporting {
  enabled: boolean;
  managed: boolean;
}

export interface ResolverOption {
  name: string;
  value: string;
  policy: string;
}

/**
 * Contains the possible string values for the secure DNS mode. This must be
 * kept in sync with the mode names in chrome/browser/net/secure_dns_config.h.
 */
export enum SecureDnsMode {
  OFF = 'off',
  AUTOMATIC = 'automatic',
  SECURE = 'secure',
}

/**
 * Contains the possible management modes. This should be kept in sync with
 * the management modes in chrome/browser/net/secure_dns_config.h.
 */
export enum SecureDnsUiManagementMode {
  NO_OVERRIDE = 0,
  DISABLED_MANAGED = 1,
  DISABLED_PARENTAL_CONTROLS = 2,
}

export interface SecureDnsSetting {
  mode: SecureDnsMode;
  config: string;
  managementMode: SecureDnsUiManagementMode;
  // <if expr="chromeos_ash">
  // Secure DNS mode and config of ChromeOS might differ with Chrome. This is
  // necessary when the DoH included or excluded domains config is set
  // (b/351091814).
  osMode: SecureDnsMode;
  osConfig: string;
  // Indicates if the templates URI contain user identifiers configured via
  // policy.
  dohWithIdentifiersActive: boolean;
  // The template URI with plain text identifiers. In the effective template
  // URI `config` the identifiers are hashed and hex encoded.
  configForDisplay: string;
  // Indicates if the DoH included or excluded domains config is configured via
  // policy.
  dohDomainConfigSet: boolean;
  // </if>
}

export interface PrivacyPageBrowserProxy {
  // <if expr="_google_chrome and not chromeos_ash">
  getMetricsReporting(): Promise<MetricsReporting>;
  setMetricsReportingEnabled(enabled: boolean): void;

  // </if>

  // <if expr="is_win or is_macosx">
  /** Invokes the native certificate manager (used by win and mac). */
  showManageSslCertificates(): void;

  // </if>

  setBlockAutoplayEnabled(enabled: boolean): void;
  getSecureDnsResolverList(): Promise<ResolverOption[]>;
  getSecureDnsSetting(): Promise<SecureDnsSetting>;

  /**
   * @return true if the config string is syntactically valid.
   */
  isValidConfig(entry: string): Promise<boolean>;

  /**
   * @return True if a test query succeeded in the specified DoH
   *     configuration or the probe was cancelled.
   */
  probeConfig(entry: string): Promise<boolean>;
}

export class PrivacyPageBrowserProxyImpl implements PrivacyPageBrowserProxy {
  // <if expr="_google_chrome and not chromeos_ash">
  getMetricsReporting() {
    return sendWithPromise('getMetricsReporting');
  }

  setMetricsReportingEnabled(enabled: boolean) {
    chrome.send('setMetricsReportingEnabled', [enabled]);
  }

  // </if>

  setBlockAutoplayEnabled(enabled: boolean) {
    chrome.send('setBlockAutoplayEnabled', [enabled]);
  }

  // <if expr="is_win or is_macosx">
  showManageSslCertificates() {
    chrome.send('showManageSSLCertificates');
  }
  // </if>

  getSecureDnsResolverList() {
    return sendWithPromise('getSecureDnsResolverList');
  }

  getSecureDnsSetting() {
    return sendWithPromise('getSecureDnsSetting');
  }

  isValidConfig(entry: string): Promise<boolean> {
    return sendWithPromise('isValidConfig', entry);
  }

  probeConfig(entry: string): Promise<boolean> {
    return sendWithPromise('probeConfig', entry);
  }

  static getInstance(): PrivacyPageBrowserProxy {
    return instance || (instance = new PrivacyPageBrowserProxyImpl());
  }

  static setInstance(obj: PrivacyPageBrowserProxy) {
    instance = obj;
  }
}

let instance: PrivacyPageBrowserProxy|null = null;
