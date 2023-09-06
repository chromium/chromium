// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview A helper object used from the internet detail dialog
 * to interact with the browser.
 */

/** @interface */
export class InternetDetailDialogBrowserProxy {
  /**
   * Returns the guid and network type as a JSON string.
   * @return {?string}
   */
  getDialogArguments() {}

  /**
   * Signals C++ that the dialog is closed.
   */
  closeDialog() {}

  /**
   * Shows the Portal Signin.
   * @param {string} guid
   */
  showPortalSignin(guid) {}
}

/**
 * @implements {InternetDetailDialogBrowserProxy}
 */
export class InternetDetailDialogBrowserProxyImpl {
  /** @override */
  getDialogArguments() {
    return chrome.getVariableValue('dialogArguments');
  }

  /** @override */
  showPortalSignin(guid) {
    chrome.send('showPortalSignin', [guid]);
  }

  /** @override */
  closeDialog() {
    chrome.send('dialogClose');
  }

  /** @return {!InternetDetailDialogBrowserProxy} */
  static getInstance() {
    return instance || (instance = new InternetDetailDialogBrowserProxyImpl());
  }

  /** @param {!InternetDetailDialogBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?InternetDetailDialogBrowserProxy} */
let instance = null;
