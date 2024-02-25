// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview A helper object used from the internet detail dialog
 * to interact with the browser.
 */

export interface InternetDetailDialogBrowserProxy {
  /**
   * @return The guid and network type as a JSON string.
   */
  getDialogArguments(): string;

  /**
   * Signals C++ that the dialog is closed.
   */
  closeDialog(): void;

  /**
   * Shows the Portal Signin.
   */
  showPortalSignin(guid: string): void;
}

export class InternetDetailDialogBrowserProxyImpl implements
    InternetDetailDialogBrowserProxy {
  getDialogArguments() {
    return chrome.getVariableValue('dialogArguments');
  }

  showPortalSignin(guid: string) {
    chrome.send('showPortalSignin', [guid]);
  }

  closeDialog() {
    chrome.send('dialogClose');
  }

  static getInstance(): InternetDetailDialogBrowserProxy {
    return instance || (instance = new InternetDetailDialogBrowserProxyImpl());
  }

  static setInstance(obj: InternetDetailDialogBrowserProxy) {
    instance = obj;
  }
}

let instance: InternetDetailDialogBrowserProxy|null = null;
