// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/ash/common/cr_deprecated.js';

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
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
addSingletonGetter(InternetDetailDialogBrowserProxyImpl);
