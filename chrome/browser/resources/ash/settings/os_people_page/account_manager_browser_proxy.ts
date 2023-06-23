// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * Information for an account managed by Chrome OS AccountManager.
 */
export interface Account {
  id: string;
  accountType: number;
  isDeviceAccount: boolean;
  isSignedIn: boolean;
  unmigrated: boolean;
  isManaged: boolean;
  fullName: string;
  email: string;
  pic: string;
  isAvailableInArc: boolean;
  organization?: string;
}

export interface AccountManagerBrowserProxy {
  /**
   * Returns a Promise for the list of GAIA accounts held in AccountManager.
   */
  getAccounts(): Promise<Account[]>;

  addAccount(): void;

  reauthenticateAccount(accountEmail: string): void;

  migrateAccount(accountEmail: string): void;

  removeAccount(account: Account): void;

  changeArcAvailability(account: Account, isAvailableInArc: boolean): void;
}

let instance: AccountManagerBrowserProxy|null = null;

export class AccountManagerBrowserProxyImpl implements
    AccountManagerBrowserProxy {
  static getInstance(): AccountManagerBrowserProxy {
    return instance || (instance = new AccountManagerBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: AccountManagerBrowserProxy): void {
    instance = obj;
  }

  getAccounts(): Promise<Account[]> {
    return sendWithPromise('getAccounts');
  }

  addAccount(): void {
    chrome.send('addAccount');
  }

  reauthenticateAccount(accountEmail: string): void {
    chrome.send('reauthenticateAccount', [accountEmail]);
  }

  migrateAccount(accountEmail: string): void {
    chrome.send('migrateAccount', [accountEmail]);
  }

  removeAccount(account: Account): void {
    chrome.send('removeAccount', [account]);
  }

  changeArcAvailability(account: Account, isAvailableInArc: boolean): void {
    chrome.send('changeArcAvailability', [account, isAvailableInArc]);
  }
}
