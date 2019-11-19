// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Google Accounts" subsection of
 * the "People" section of Settings, to interact with the browser. Chrome OS
 * only.
 */
cr.exportPath('settings');

/**
 * Information for an account managed by Chrome OS AccountManager.
 * @typedef {{
 *   id: string,
 *   accountType: number,
 *   isDeviceAccount: boolean,
 *   isSignedIn: boolean,
 *   unmigrated: boolean,
 *   fullName: string,
 *   email: string,
 *   pic: string,
 *   organization: (string|undefined),
 * }}
 */
settings.Account;

cr.define('settings', function() {
  /** @interface */
  class AccountManagerBrowserProxy {
    /**
     * Returns a Promise for the list of GAIA accounts held in AccountManager.
     * @return {!Promise<!Array<settings.Account>>}
     */
    getAccounts() {}

    /**
     * Triggers the 'Add account' flow.
     */
    addAccount() {}

    /**
     * Triggers the re-authentication flow for the account pointed to by
     * |account_email|.
     * @param {string} account_email
     */
    reauthenticateAccount(account_email) {}

    /**
     * Triggers the migration dialog for the account pointed to by
     * |account_email|.
     * @param {string} account_email
     */
    migrateAccount(account_email) {}

    /**
     * Removes |account| from Account Manager.
     * @param {?settings.Account} account
     */
    removeAccount(account) {}

    /**
     * Displays the Account Manager welcome dialog if required.
     */
    showWelcomeDialogIfRequired() {}
  }

  /**
   * @implements {settings.AccountManagerBrowserProxy}
   */
  class AccountManagerBrowserProxyImpl {
    /** @override */
    getAccounts() {
      return cr.sendWithPromise('getAccounts');
    }

    /** @override */
    addAccount() {
      chrome.send('addAccount');
    }

    /** @override */
    reauthenticateAccount(account_email) {
      chrome.send('reauthenticateAccount', [account_email]);
    }

    /** @override */
    migrateAccount(account_email) {
      chrome.send('migrateAccount', [account_email]);
    }

    /** @override */
    removeAccount(account) {
      chrome.send('removeAccount', [account]);
    }

    /** @override */
    showWelcomeDialogIfRequired() {
      chrome.send('showWelcomeDialogIfRequired');
    }
  }

  cr.addSingletonGetter(AccountManagerBrowserProxyImpl);

  return {
    AccountManagerBrowserProxy: AccountManagerBrowserProxy,
    AccountManagerBrowserProxyImpl: AccountManagerBrowserProxyImpl,
  };
});
