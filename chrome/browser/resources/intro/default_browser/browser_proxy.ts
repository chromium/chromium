// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the chrome://intro/default-browser page
 * to interact with the browser.
 */

export interface DefaultBrowserBrowserProxy {
  // Called when the user clicks on the "Set as default" button.
  setAsDefaultBrowser(): void;

  // Called when the user clicks on the "Skip" button.
  skipDefaultBrowser(): void;
}

export class DefaultBrowserBrowserProxyImpl implements
    DefaultBrowserBrowserProxy {
  setAsDefaultBrowser() {
    chrome.send('setAsDefaultBrowser');
  }


  skipDefaultBrowser() {
    chrome.send('skipDefaultBrowser');
  }

  static getInstance(): DefaultBrowserBrowserProxy {
    return instance || (instance = new DefaultBrowserBrowserProxyImpl());
  }

  static setInstance(obj: DefaultBrowserBrowserProxy) {
    instance = obj;
  }
}

let instance: DefaultBrowserBrowserProxy|null = null;
