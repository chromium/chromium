// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
export class OsResetBrowserProxy {
  /**
   * A method to be called when the reset powerwash dialog is shown.
   */
  onPowerwashDialogShow() {}

  /**
   * Initiates a factory reset and restarts.
   */
  requestFactoryResetRestart() {}
}

/** @type {?OsResetBrowserProxy} */
let instance = null;

/**
 * @implements {OsResetBrowserProxy}
 */
export class OsResetBrowserProxyImpl {
  /** @return {!OsResetBrowserProxy} */
  static getInstance() {
    return instance || (instance = new OsResetBrowserProxyImpl());
  }

  /** @param {!OsResetBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  onPowerwashDialogShow() {
    chrome.send('onPowerwashDialogShow');
  }

  /** @override */
  requestFactoryResetRestart() {
    chrome.send('requestFactoryResetRestart');
  }
}
