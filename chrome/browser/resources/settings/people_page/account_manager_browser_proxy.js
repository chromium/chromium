// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

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
  export let Account;

  /** @interface */
  export class AccountManagerBrowserProxy {
    /**
     * Returns a Promise for the list of GAIA accounts held in AccountManager.
     * @return {!Promise<!Array<Account>>}
     */
    getAccounts() {}

    /**
     * Triggers the 'Add account' flow.
     */
    addAccount() {}

    /**
     * Triggers the re-authentication flow for the account pointed to by
     * |accountEmail|.
     * @param {string} accountEmail
     */
    reauthenticateAccount(accountEmail) {}

    /**
     * Triggers the migration dialog for the account pointed to by
     * |accountEmail|.
     * @param {string} accountEmail
     */
    migrateAccount(accountEmail) {}

    /**
     * Removes |account| from Account Manager.
     * @param {?Account} account
     */
    removeAccount(account) {}

    /**
     * Displays the Account Manager welcome dialog if required.
     */
    showWelcomeDialogIfRequired() {}
  }

  /**
   * @implements {AccountManagerBrowserProxy}
   */
  export class AccountManagerBrowserProxyImpl {
    /** @override */
    getAccounts() {
      return sendWithPromise('getAccounts');
    }

    /** @override */
    addAccount() {
      chrome.send('addAccount');
    }

    /** @override */
    reauthenticateAccount(accountEmail) {
      chrome.send('reauthenticateAccount', [accountEmail]);
    }

    /** @override */
    migrateAccount(accountEmail) {
      chrome.send('migrateAccount', [accountEmail]);
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

  addSingletonGetter(AccountManagerBrowserProxyImpl);

