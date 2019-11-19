// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class BrowserSwitchProxy {
  /**
   * @param {string} url URL to open in alternative browser.
   * @return {Promise} A promise that can fail if unable to launch. It will
   *     never resolve, because the tab closes if this succeeds.
   */
  launchAlternativeBrowserAndCloseTab(url) {}

  gotoNewTabPage() {}
}

/** @implements {BrowserSwitchProxy} */
export class BrowserSwitchProxyImpl {
  /** @override */
  launchAlternativeBrowserAndCloseTab(url) {
    return sendWithPromise('launchAlternativeBrowserAndCloseTab', url);
  }

  /** @override */
  gotoNewTabPage() {
    chrome.send('gotoNewTabPage');
  }
}

addSingletonGetter(BrowserSwitchProxyImpl);
