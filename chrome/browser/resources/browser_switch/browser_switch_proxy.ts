// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/** @interface */
export interface BrowserSwitchProxy {
  /**
   * @param URL to open in alternative browser.
   * @return A promise that can fail if unable to launch. It will never resolve,
   *     because the tab closes if this succeeds.
   */
  launchAlternativeBrowserAndCloseTab(url: string): Promise<void>;

  gotoNewTabPage(): void;
}

export class BrowserSwitchProxyImpl implements BrowserSwitchProxy {
  launchAlternativeBrowserAndCloseTab(url: string) {
    return sendWithPromise('launchAlternativeBrowserAndCloseTab', url);
  }

  gotoNewTabPage() {
    chrome.send('gotoNewTabPage');
  }

  static getInstance(): BrowserSwitchProxy {
    return instance || (instance = new BrowserSwitchProxyImpl());
  }
}

let instance: BrowserSwitchProxy|null = null;
