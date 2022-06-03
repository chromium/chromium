// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

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

/**
 * @implements {OsResetBrowserProxy}
 */
export class OsResetBrowserProxyImpl {
  /** @override */
  onPowerwashDialogShow() {
    chrome.send('onPowerwashDialogShow');
  }

  /** @override */
  requestFactoryResetRestart() {
    chrome.send('requestFactoryResetRestart');
  }
}

addSingletonGetter(OsResetBrowserProxyImpl);
