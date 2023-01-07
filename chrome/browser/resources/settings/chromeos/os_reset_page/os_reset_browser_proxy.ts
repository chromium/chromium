// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface OsResetBrowserProxy {
  /**
   * A method to be called when the reset powerwash dialog is shown.
   */
  onPowerwashDialogShow(): void;

  /**
   * Initiates a factory reset and restarts.
   */
  requestFactoryResetRestart(): void;
}

let instance: OsResetBrowserProxy|null = null;

export class OsResetBrowserProxyImpl implements OsResetBrowserProxy {
  static getInstance(): OsResetBrowserProxy {
    return instance || (instance = new OsResetBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: OsResetBrowserProxy) {
    instance = obj;
  }

  onPowerwashDialogShow() {
    chrome.send('onPowerwashDialogShow');
  }

  requestFactoryResetRestart() {
    chrome.send('requestFactoryResetRestart');
  }
}
