// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Kerberos Accounts" subsection of
 * the "Kerberos" section of Settings, to interact with the browser. Chrome OS
 * only.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * Information for a Chrome OS Kerberos account.
 */
export interface KerberosAccount {
  principalName: string;
  config: string;
  isSignedIn: boolean;
  isActive: boolean;
  isManaged: boolean;
  passwordWasRemembered: boolean;
  pic: string;
  validForDuration: string;
}

export interface ValidateKerberosConfigResult {
  error: KerberosErrorType;
  errorInfo: {code: KerberosConfigErrorCode, lineIndex: number|undefined};
}

/**
 *  These values must be kept in sync with the ErrorType enum in
 *  third_party/cros_system_api/dbus/kerberos/kerberos_service.proto.
 */
export enum KerberosErrorType {
  NONE = 0,
  UNKNOWN = 1,
  D_BUS_FAILURE = 2,
  NETWORK_PROBLEM = 3,
  UNKNOWN_KRB5_ERROR = 4,
  BAD_PRINCIPAL = 5,
  BAD_PASSWORD = 6,
  PASSWORD_EXPIRED = 7,
  PASSWORD_REJECTED = 8,
  NO_CREDENTIALS_CACHE_FOUND = 9,
  KERBEROS_TICKET_EXPIRED = 10,
  KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE = 11,
  CONTACTING_KDC_FAILED = 12,
  PARSE_REQUEST_FAILED = 13,
  LOCAL_IO = 14,
  UNKNOWN_PRINCIPAL_NAME = 15,
  DUPLICATE_PRINCIPAL_NAME = 16,
  IN_PROGRESS = 17,
  PARSE_PRINCIPAL_FAILED = 18,
  BAD_CONFIG = 19,
  JAIL_FAILURE = 20,
  KERBEROS_DISABLED = 21,
}

/**
 *  Error codes for config validation.
 *  These values must be kept in sync with the KerberosConfigErrorCode enum in
 *  third_party/cros_system_api/dbus/kerberos/kerberos_service.proto.
 */
export enum KerberosConfigErrorCode {
  NONE = 0,
  SECTION_NESTED_IN_GROUP = 1,
  SECTION_SYNTAX = 2,
  EXPECTED_OPENING_CURLY_BRACE = 3,
  EXTRA_CURLY_BRACE = 4,
  RELATION_SYNTAX = 5,
  KEY_NOT_SUPPORTED = 6,
  SECTION_NOT_SUPPORTED = 7,
  KRB5_FAILED_TO_PARSE = 8,
  TOO_MANY_NESTED_GROUPS = 9,
  LINE_TOO_LONG = 10,
}

export interface KerberosAccountsBrowserProxy {
  /**
   * Returns a Promise for the list of Kerberos accounts held in the kerberosd
   * system daemon.
   */
  getAccounts(): Promise<KerberosAccount[]>;

  /**
   * Attempts to add a new (or update an existing) Kerberos account.
   * @param principalName Kerberos principal (user@realm.com).
   * @param password Account password.
   * @param rememberPassword Whether to store the password.
   * @param config Kerberos configuration.
   * @param allowExisting Whether existing accounts may be updated.
   */
  addAccount(
      principalName: string, password: string, rememberPassword: boolean,
      config: string, allowExisting: boolean): Promise<KerberosErrorType>;

  /**
   * Removes |account| from the set of Kerberos accounts.
   */
  removeAccount(account: KerberosAccount|null): Promise<KerberosErrorType>;

  /**
   * Validates |krb5conf| by making sure that it does not contain syntax
   *     errors or disallowed configuration options.
   * @param krb5Conf Kerberos configuration data (krb5.conf)
   */
  validateConfig(krb5Conf: string): Promise<ValidateKerberosConfigResult>;

  /**
   * Sets |account| as currently active account. Kerberos credentials are
   * consumed from this account.
   */
  setAsActiveAccount(account: KerberosAccount|null): void;
}

let instance: KerberosAccountsBrowserProxy|null = null;

export class KerberosAccountsBrowserProxyImpl implements
    KerberosAccountsBrowserProxy {
  static getInstance(): KerberosAccountsBrowserProxy {
    return instance || (instance = new KerberosAccountsBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: KerberosAccountsBrowserProxy): void {
    instance = obj;
  }

  getAccounts(): Promise<KerberosAccount[]> {
    return sendWithPromise('getKerberosAccounts');
  }

  addAccount(
      principalName: string, password: string, rememberPassword: boolean,
      config: string, allowExisting: boolean): Promise<KerberosErrorType> {
    return sendWithPromise(
        'addKerberosAccount', principalName, password, rememberPassword, config,
        allowExisting);
  }

  removeAccount(account: KerberosAccount): Promise<KerberosErrorType> {
    return sendWithPromise('removeKerberosAccount', account!.principalName);
  }

  validateConfig(krb5conf: string): Promise<ValidateKerberosConfigResult> {
    return sendWithPromise('validateKerberosConfig', krb5conf);
  }

  setAsActiveAccount(account: KerberosAccount): void {
    chrome.send('setAsActiveKerberosAccount', [account!.principalName]);
  }
}
