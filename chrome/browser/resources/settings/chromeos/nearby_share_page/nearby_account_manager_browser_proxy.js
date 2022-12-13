// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';

// TODO(crbug.com/1123712): Add a message handler for this class instead of
// implicitly relying on the People Page.
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
export class NearbyAccountManagerBrowserProxy {
  /**
   * Returns a Promise for the list of GAIA accounts held in AccountManager.
   * @return {!Promise<!Array<Account>>}
   */
  getAccounts() {}
}

/** @type {?NearbyAccountManagerBrowserProxy} */
let instance = null;

/**
 * @implements {NearbyAccountManagerBrowserProxy}
 */
export class NearbyAccountManagerBrowserProxyImpl {
  /** @return {!NearbyAccountManagerBrowserProxy} */
  static getInstance() {
    return instance || (instance = new NearbyAccountManagerBrowserProxyImpl());
  }

  /** @param {!NearbyAccountManagerBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  getAccounts() {
    return sendWithPromise('getAccounts');
  }
}
