// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Stripped down fork of
 * c/b/r/ash/settings/os_people_page/account_manager_browser_proxy.js.
 * Re-uses the same WebUI message handler class.
 */

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

/**
 * Information for an account managed by Chrome OS AccountManager.
 */
export interface Account {
  id: string;
  accountType: number;
  isDeviceAccount: boolean;
  isSignedIn: boolean;
  unmigrated: boolean;
  fullName: string;
  email: string;
  pic: string;
  organization?: string;
}

export interface AccountManagerBrowserProxy {
  /**
   * Returns a Promise for the list of GAIA accounts held in AccountManager.
   */
  getAccounts(): Promise<Account[]>;
}

export class AccountManagerBrowserProxyImpl implements
    AccountManagerBrowserProxy {
  getAccounts() {
    return sendWithPromise('getAccounts');
  }

  static getInstance(): AccountManagerBrowserProxy {
    return instance || (instance = new AccountManagerBrowserProxyImpl());
  }

  static setInstance(obj: AccountManagerBrowserProxy) {
    instance = obj;
  }
}

let instance: AccountManagerBrowserProxy|null = null;
