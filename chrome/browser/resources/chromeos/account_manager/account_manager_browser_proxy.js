// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Functions for Account manager screens.
 */

/** @interface */
export class AccountManagerBrowserProxy {
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
 * @implements {AccountManagerBrowserProxy}
 */
export class AccountManagerBrowserProxyImpl {
  /** @override */
  reauthenticateAccount(account_email) {
    chrome.send('reauthenticateAccount', [account_email]);
  }

  /** @override */
  closeDialog() {
    chrome.send('closeDialog');
  }

  /** @return {!AccountManagerBrowserProxy} */
  static getInstance() {
    return instance || (instance = new AccountManagerBrowserProxyImpl());
  }

  /** @param {!AccountManagerBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?AccountManagerBrowserProxy} */
let instance = null;
