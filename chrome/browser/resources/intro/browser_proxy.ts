// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the chrome://intro page to
 * interact with the browser.
 */

export interface IntroBrowserProxy {
  // Called when the user clicks the "sign in" button.
  continueWithAccount(): void;

  // Called when the user clicks the "continue without account" button.
  continueWithoutAccount(): void;

  // Initializes the FRE intro main view.
  initializeMainView(): void;
}

export class IntroBrowserProxyImpl implements IntroBrowserProxy {
  continueWithAccount() {
    chrome.send('continueWithAccount');
  }

  continueWithoutAccount() {
    chrome.send('continueWithoutAccount');
  }

  initializeMainView() {
    chrome.send('initializeMainView');
  }

  static getInstance(): IntroBrowserProxy {
    return instance || (instance = new IntroBrowserProxyImpl());
  }

  static setInstance(obj: IntroBrowserProxy) {
    instance = obj;
  }
}

let instance: IntroBrowserProxy|null = null;
