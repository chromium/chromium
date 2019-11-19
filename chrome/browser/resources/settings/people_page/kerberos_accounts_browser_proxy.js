// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Kerberos Accounts" subsection of
 * the "People" section of Settings, to interact with the browser. Chrome OS
 * only.
 */
cr.exportPath('settings');

/**
 * Information for a Chrome OS Kerberos account.
 * @typedef {{
 *   principalName: string,
 *   config: string,
 *   isSignedIn: boolean,
 *   isActive: boolean,
 *   isManaged: boolean,
 *   passwordWasRemembered: boolean,
 *   pic: string,
 *   validForDuration: string
 * }}
 */
settings.KerberosAccount;

/**
 * @typedef {{
 *   error: !settings.KerberosErrorType,
 *   errorInfo: !{
 *     code: !settings.KerberosConfigErrorCode,
 *     lineIndex: (number|undefined)
 *   }
 * }}
 */
settings.ValidateKerberosConfigResult;

cr.define('settings', function() {
  /**
   *  @enum {number}
   *  These values must be kept in sync with the ErrorType enum in
   *  third_party/cros_system_api/dbus/kerberos/kerberos_service.proto.
   */
  const KerberosErrorType = {
    kNone: 0,
    kUnknown: 1,
    kDBusFailure: 2,
    kNetworkProblem: 3,
    kUnknownKrb5Error: 4,
    kBadPrincipal: 5,
    kBadPassword: 6,
    kPasswordExpired: 7,
    kPasswordRejected: 8,
    kNoCredentialsCacheFound: 9,
    kKerberosTicketExpired: 10,
    kKdcDoesNotSupportEncryptionType: 11,
    kContactingKdcFailed: 12,
    kParseRequestFailed: 13,
    kLocalIo: 14,
    kUnknownPrincipalName: 15,
    kDuplicatePrincipalName: 16,
    kInProgress: 17,
    kParsePrincipalFailed: 18,
    kBadConfig: 19,
    kJailFailure: 20,
  };

  /**
   *  @enum {number}
   *  Error codes for config validation.
   *  These values must be kept in sync with the KerberosConfigErrorCode enum in
   *  third_party/cros_system_api/dbus/kerberos/kerberos_service.proto.
   */
  const KerberosConfigErrorCode = {
    kNone: 0,
    kSectionNestedInGroup: 1,
    kSectionSyntax: 2,
    kExpectedOpeningCurlyBrace: 3,
    kExtraCurlyBrace: 4,
    kRelationSyntax: 5,
    kKeyNotSupported: 6,
    kSectionNotSupported: 7,
    kKrb5FailedToParse: 8,
  };

  /** @interface */
  class KerberosAccountsBrowserProxy {
    /**
     * Returns a Promise for the list of Kerberos accounts held in the kerberosd
     * system daemon.
     * @return {!Promise<!Array<!settings.KerberosAccount>>}
     */
    getAccounts() {}

    /**
     * Attempts to add a new (or update an existing) Kerberos account.
     * @param {string} principalName Kerberos principal (user@realm.com).
     * @param {string} password Account password.
     * @param {boolean} rememberPassword Whether to store the password.
     * @param {string} config Kerberos configuration.
     * @param {boolean} allowExisting Whether existing accounts may be updated.
     * @return {!Promise<!settings.KerberosErrorType>}
     */
    addAccount(
        principalName, password, rememberPassword, config, allowExisting) {}

    /**
     * Removes |account| from the set of Kerberos accounts.
     * @param {!settings.KerberosAccount} account
     * @return {!Promise<!settings.KerberosErrorType>}
     */
    removeAccount(account) {}

    /**
     * Validates |krb5conf| by making sure that it does not contain syntax
     *     errors or disallowed configuration options.
     * @param {string} krb5Conf Kerberos configuration data (krb5.conf)
     * @return {!Promise<!settings.ValidateKerberosConfigResult>}
     */
    validateConfig(krb5Conf) {}

    /**
     * Sets |account| as currently active account. Kerberos credentials are
     * consumed from this account.
     * @param {!settings.KerberosAccount} account
     */
    setAsActiveAccount(account) {}
  }

  /**
   * @implements {settings.KerberosAccountsBrowserProxy}
   */
  class KerberosAccountsBrowserProxyImpl {
    /** @override */
    getAccounts() {
      return cr.sendWithPromise('getKerberosAccounts');
    }

    /** @override */
    addAccount(
        principalName, password, rememberPassword, config, allowExisting) {
      return cr.sendWithPromise(
          'addKerberosAccount', principalName, password, rememberPassword,
          config, allowExisting);
    }

    /** @override */
    removeAccount(account) {
      return cr.sendWithPromise('removeKerberosAccount', account.principalName);
    }

    /** @override */
    validateConfig(krb5conf) {
      return cr.sendWithPromise('validateKerberosConfig', krb5conf);
    }

    /** @override */
    setAsActiveAccount(account) {
      chrome.send('setAsActiveKerberosAccount', [account.principalName]);
    }
  }

  cr.addSingletonGetter(KerberosAccountsBrowserProxyImpl);

  return {
    KerberosErrorType: KerberosErrorType,
    KerberosConfigErrorCode: KerberosConfigErrorCode,
    KerberosAccountsBrowserProxy: KerberosAccountsBrowserProxy,
    KerberosAccountsBrowserProxyImpl: KerberosAccountsBrowserProxyImpl,
  };
});
