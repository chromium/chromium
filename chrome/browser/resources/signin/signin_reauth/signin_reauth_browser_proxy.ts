// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the signin reauth dialog to
 * interact with the browser.
 */

export interface SigninReauthBrowserProxy {
  /**
   * Called when the app has been initialized.
   */
  initialize(): void;

  /**
   * Called when the user confirms the signin reauth dialog.
   * @param description Strings that the user was presented with in the UI.
   * @param confirmation Text of the element that the user clicked on.
   */
  confirm(description: string[], confirmation: string): void;

  /**
   * Called when the user cancels the signin reauth.
   */
  cancel(): void;
}

export class SigninReauthBrowserProxyImpl implements SigninReauthBrowserProxy {
  initialize() {
    chrome.send('initialize');
  }

  confirm(description: string[], confirmation: string) {
    chrome.send('confirm', [description, confirmation]);
  }

  cancel() {
    chrome.send('cancel');
  }

  static getInstance(): SigninReauthBrowserProxy {
    return instance || (instance = new SigninReauthBrowserProxyImpl());
  }

  static setInstance(obj: SigninReauthBrowserProxy) {
    instance = obj;
  }
}

let instance: SigninReauthBrowserProxy|null = null;
