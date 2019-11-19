// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Functions for Account migration flow.
 */
cr.define('account_migration', function() {
  /** @interface */
  class AccountMigrationBrowserProxy {
    /**
     * Triggers the re-authentication flow for the account pointed to by
     * |account_email|.
     * @param {string} account_email
     */
    reauthenticateAccount(account_email) {}

    /**
     * Closes the dialog.
     */
    closeDialog() {}
  }

  /**
   * @implements {settings.AccountMigrationBrowserProxy}
   */
  class AccountMigrationBrowserProxyImpl {
    /** @override */
    reauthenticateAccount(account_email) {
      chrome.send('reauthenticateAccount', [account_email]);
    }

    /** @override */
    closeDialog() {
      chrome.send('closeDialog');
    }
  }

  cr.addSingletonGetter(AccountMigrationBrowserProxyImpl);

  return {
    AccountMigrationBrowserProxy: AccountMigrationBrowserProxy,
    AccountMigrationBrowserProxyImpl: AccountMigrationBrowserProxyImpl,
  };
});
