// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles interprocess communication for the security page. */

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

export interface ResolverOption {
  name: string;
  value: string;
  policy: string;
}

/**
 * Contains the possible string values for the secure DNS mode. This must be
 * kept in sync with the mode names in chrome/browser/net/secure_dns_config.h.
 */
// LINT.IfChange(SecureDnsMode)
export enum SecureDnsMode {
  OFF = 'off',
  AUTOMATIC = 'automatic',
  SECURE = 'secure',
}
// LINT.ThenChange(//chrome/browser/net/secure_dns_config.h:SecureDnsMode)

/**
 * Contains the possible management modes. This should be kept in sync with
 * the management modes in chrome/browser/net/secure_dns_config.h.
 */
// LINT.IfChange(SecureDnsUiManagementMode)
export enum SecureDnsUiManagementMode {
  NO_OVERRIDE = 0,
  DISABLED_MANAGED = 1,
  DISABLED_PARENTAL_CONTROLS = 2,
}
// LINT.ThenChange(//chrome/browser/net/secure_dns_config.h:SecureDnsUiManagementMode)

/** Contains the current secure DNS configuration and its status. */
export interface SecureDnsSetting {
  mode: SecureDnsMode;
  config: string;
  managementMode: SecureDnsUiManagementMode;
  // <if expr="is_chromeos">
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

/** Handles interprocess communication for the security page. */
export interface SecurityPageBrowserProxy {
  /** @return A list of possible secure DNS providers. */
  getSecureDnsResolverList(): Promise<ResolverOption[]>;

  /** @return The current secure DNS setting. */
  getSecureDnsSetting(): Promise<SecureDnsSetting>;

  /**
   * @param entry The config string to validate.
   * @return True if the config string is syntactically valid.
   */
  isValidConfig(entry: string): Promise<boolean>;

  /**
   * @param entry The config string to probe.
   * @return True if a test query succeeded in the specified DoH
   *     configuration or the probe was cancelled.
   */
  probeConfig(entry: string): Promise<boolean>;
}

export class SecurityPageBrowserProxyImpl implements SecurityPageBrowserProxy {
  getSecureDnsResolverList(): Promise<ResolverOption[]> {
    return sendWithPromise<ResolverOption[]>('getSecureDnsResolverList');
  }

  getSecureDnsSetting(): Promise<SecureDnsSetting> {
    return sendWithPromise<SecureDnsSetting>('getSecureDnsSetting');
  }

  isValidConfig(entry: string): Promise<boolean> {
    return sendWithPromise<boolean>('isValidConfig', entry);
  }

  probeConfig(entry: string): Promise<boolean> {
    return sendWithPromise<boolean>('probeConfig', entry);
  }

  static getInstance(): SecurityPageBrowserProxy {
    return instance || (instance = new SecurityPageBrowserProxyImpl());
  }

  static setInstance(obj: SecurityPageBrowserProxy) {
    instance = obj;
  }
}

let instance: SecurityPageBrowserProxy|null = null;
