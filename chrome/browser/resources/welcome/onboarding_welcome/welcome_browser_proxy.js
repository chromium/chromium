// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the welcome page to interact with
 * the browser.
 */

cr.define('welcome', function() {

  /** @interface */
  class WelcomeBrowserProxy {
    /** @param {?string} redirectUrl the URL to go to, after signing in. */
    handleActivateSignIn(redirectUrl) {}

    /** @param {boolean=} replace */
    goToNewTabPage(replace) {}

    /** @param {string} url */
    goToURL(url) {}
  }

  /** @implements {welcome.WelcomeBrowserProxy} */
  class WelcomeBrowserProxyImpl {
    /** @override */
    handleActivateSignIn(redirectUrl) {
      chrome.send('handleActivateSignIn', redirectUrl ? [redirectUrl] : []);
    }

    /** @override */
    goToNewTabPage(replace) {
      if (replace)
        window.location.replace('chrome://newtab');
      else
        window.location.assign('chrome://newtab');
    }

    /** @override */
    goToURL(url) {
      window.location.assign(url);
    }
  }

  cr.addSingletonGetter(WelcomeBrowserProxyImpl);

  return {
    WelcomeBrowserProxy: WelcomeBrowserProxy,
    WelcomeBrowserProxyImpl: WelcomeBrowserProxyImpl,
  };
});
