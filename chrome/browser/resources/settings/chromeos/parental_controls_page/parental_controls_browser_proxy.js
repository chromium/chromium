// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Browser Proxy for Parental Controls functions.
 */

/** @interface */
export class ParentalControlsBrowserProxy {
  /**
   * Shows the Add Supervsion dialog.
   */
  showAddSupervisionDialog() {}

  /**
   * Launches an app that shows the Family Link Settings.  Depending
   * on whether the Family Link Helper app is available, this might
   * launch the app, or take some kind of backup/default action.
   */
  launchFamilyLinkSettings() {}
}

/** @type {?ParentalControlsBrowserProxy} */
let instance = null;

/** @implements {ParentalControlsBrowserProxy} */
export class ParentalControlsBrowserProxyImpl {
  /** @return {!ParentalControlsBrowserProxy} */
  static getInstance() {
    return instance || (instance = new ParentalControlsBrowserProxyImpl());
  }

  /** @param {!ParentalControlsBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  showAddSupervisionDialog() {
    chrome.send('showAddSupervisionDialog');
  }

  /** @override */
  launchFamilyLinkSettings() {
    chrome.send('launchFamilyLinkSettings');
  }
}
