// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview A helper object used by the welcome page to interact with
 * the browser.
 */

/** @interface */
export class WelcomeBrowserProxy {
  /** @param {?string} redirectUrl the URL to go to, after signing in. */
  handleActivateSignIn(redirectUrl) {}

  handleUserDecline() {}

  /** @param {boolean=} replace */
  goToNewTabPage(replace) {}

  /** @param {string} url */
  goToURL(url) {}
}

/** @implements {WelcomeBrowserProxy} */
export class WelcomeBrowserProxyImpl {
  /** @override */
  handleActivateSignIn(redirectUrl) {
    chrome.send('handleActivateSignIn', redirectUrl ? [redirectUrl] : []);
  }

  /** @override */
  handleUserDecline() {
    chrome.send('handleUserDecline');
  }

  /** @override */
  goToNewTabPage(replace) {
    if (replace) {
      window.location.replace('chrome://newtab');
    } else {
      window.location.assign('chrome://newtab');
    }
  }

  /** @override */
  goToURL(url) {
    window.location.assign(url);
  }
}

addSingletonGetter(WelcomeBrowserProxyImpl);
