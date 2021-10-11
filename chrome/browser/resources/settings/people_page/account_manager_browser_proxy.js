// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Stripped down fork of
 * c/b/r/settings/chromeos/os_people_page/account_manager_browser_proxy.js.
 * Re-uses the same WebUI message handler class.
 */

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
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
  }

  /**
   * @implements {AccountManagerBrowserProxy}
   */
  export class AccountManagerBrowserProxyImpl {
    /** @override */
    getAccounts() {
      return sendWithPromise('getAccounts');
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
