// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used to open a URL in a new tab.
 * the browser.
 */

/** @interface */
export class OpenWindowProxy {
  /**
   * Opens the specified URL in a new tab.
   * @param {string} url
   */
  openURL(url) {}
}

/** @implements {OpenWindowProxy} */
export class OpenWindowProxyImpl {
  /** @override */
  openURL(url) {
    window.open(url);
  }

  /** @return {!OpenWindowProxy} */
  static getInstance() {
    return instance || (instance = new OpenWindowProxyImpl());
  }

  /** @param {!OpenWindowProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?OpenWindowProxy} */
let instance = null;
