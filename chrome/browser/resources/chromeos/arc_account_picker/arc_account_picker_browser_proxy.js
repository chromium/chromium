// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';

/**
 * Data representing a Gaia account added in-session.
 * @typedef {{
 *   id: string,
 *   email: string,
 *   fullName: string,
 *   image: string,
 * }}
 */
export let Account;

/** @interface */
export class ArcAccountPickerBrowserProxy {
  /**
   * Send 'getAccountsNotAvailableInArc' message to the handler. The promise
   * will be resolved with the list of accounts that are not available in ARC.
   * @return {Promise<Array<Account>>}
   */
  getAccountsNotAvailableInArc() {}

  /**
   * @param {Account} account
   */
  makeAvailableInArc(account) {}
}

/** @implements {ArcAccountPickerBrowserProxy} */
export class ArcAccountPickerBrowserProxyImpl {
  /** @override */
  getAccountsNotAvailableInArc() {
    return sendWithPromise('getAccountsNotAvailableInArc');
  }

  /** @override */
  makeAvailableInArc(account) {
    chrome.send('makeAvailableInArc', [account]);
  }

  /** @return {!ArcAccountPickerBrowserProxy} */
  static getInstance() {
    return instance || (instance = new ArcAccountPickerBrowserProxyImpl());
  }

  /** @param {!ArcAccountPickerBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?ArcAccountPickerBrowserProxy} */
let instance = null;
