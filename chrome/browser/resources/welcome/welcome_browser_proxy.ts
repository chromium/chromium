// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the welcome page to interact with
 * the browser.
 */
export interface WelcomeBrowserProxy {
  /** @param redirectUrl the URL to go to, after signing in. */
  handleActivateSignIn(redirectUrl: string|null): void;

  handleUserDecline(): void;
  goToNewTabPage(replace?: boolean): void;
  goToUrl(url: string): void;
}

export class WelcomeBrowserProxyImpl implements WelcomeBrowserProxy {
  handleActivateSignIn(redirectUrl: string|null): void {
    chrome.send('handleActivateSignIn', redirectUrl ? [redirectUrl] : []);
  }

  handleUserDecline(): void {
    chrome.send('handleUserDecline');
  }

  goToNewTabPage(replace?: boolean): void {
    if (replace) {
      window.location.replace('chrome://newtab');
    } else {
      window.location.assign('chrome://newtab');
    }
  }

  goToUrl(url: string): void {
    window.location.assign(url);
  }

  static getInstance(): WelcomeBrowserProxy {
    return instance || (instance = new WelcomeBrowserProxyImpl());
  }

  static setInstance(obj: WelcomeBrowserProxy) {
    instance = obj;
  }
}

let instance: WelcomeBrowserProxy|null = null;
