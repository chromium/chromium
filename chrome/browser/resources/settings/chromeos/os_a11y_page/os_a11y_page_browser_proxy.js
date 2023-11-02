// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
export class OsA11yPageBrowserProxy {
  /**
   * Requests whether screen reader state changed. Result
   * is returned by the 'screen-reader-state-changed' WebUI listener event.
   */
  a11yPageReady() {}

  /**
   * Opens the a11y image labels modal dialog.
   */
  confirmA11yImageLabels() {}
}

/** @type {?OsA11yPageBrowserProxy} */
let instance = null;

/**
 * @implements {OsA11yPageBrowserProxy}
 */
export class OsA11yPageBrowserProxyImpl {
  /** @return {!OsA11yPageBrowserProxy} */
  static getInstance() {
    return instance || (instance = new OsA11yPageBrowserProxyImpl());
  }

  /** @param {!OsA11yPageBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  a11yPageReady() {
    chrome.send('a11yPageReady');
  }

  /** @override */
  confirmA11yImageLabels() {
    chrome.send('confirmA11yImageLabels');
  }
}
