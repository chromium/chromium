// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

// TODO(crbug.com/1123712): Add a message handler for this class instead of
// implicitly relying on the People Page.
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
  organization: string|undefined;
}

export interface NearbyAccountManagerBrowserProxy {
  /**
   * Returns a Promise for the list of GAIA accounts held in AccountManager.
   */
  getAccounts(): Promise<Account[]>;
}

let instance: NearbyAccountManagerBrowserProxy|null = null;

export class NearbyAccountManagerBrowserProxyImpl implements
    NearbyAccountManagerBrowserProxy {
  static getInstance(): NearbyAccountManagerBrowserProxy {
    return instance || (instance = new NearbyAccountManagerBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: NearbyAccountManagerBrowserProxy): void {
    instance = obj;
  }

  getAccounts(): Promise<Account[]> {
    return sendWithPromise('getAccounts');
  }
}
