// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/ash/common/cr_deprecated.js';

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
}

addSingletonGetter(AccountManagerBrowserProxyImpl);
