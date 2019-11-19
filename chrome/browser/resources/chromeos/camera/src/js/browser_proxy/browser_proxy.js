// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for proxy.
 */
cca.proxy = cca.proxy || {};

(function() {
/**
 * The Chrome App implementation of the CCA's interaction with the browser.
 * @implements {cca.proxy.BrowserProxy}
 */
class ChromeAppBrowserProxy {
  /** @override */
  getVolumeList(callback) {
    chrome.fileSystem.getVolumeList(callback);
  }

  /** @override */
  requestFileSystem(options, callback) {
    chrome.fileSystem.requestFileSystem(options, callback);
  }

  /** @override */
  localStorageGet(keys, callback) {
    chrome.storage.local.get(keys, callback);
  }

  /** @override */
  localStorageSet(items, callback) {
    chrome.storage.local.set(items, callback);
  }

  /** @override */
  localStorageRemove(items, callback) {
    chrome.storage.local.remove(items, callback);
  }
}

/**
 * Namespace for browser functions.
 * @type {cca.proxy.BrowserProxy}
 */
cca.proxy.browserProxy = new ChromeAppBrowserProxy();
})();
