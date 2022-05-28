// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
export class PersonalizationHubBrowserProxy {
  openPersonalizationHub() {}
}

/** @type {?PersonalizationHubBrowserProxy} */
let instance = null;

/**
 * @implements {PersonalizationHubBrowserProxy}
 */
export class PersonalizationHubBrowserProxyImpl {
  /** @return {!PersonalizationHubBrowserProxy} */
  static getInstance() {
    return instance || (instance = new PersonalizationHubBrowserProxyImpl());
  }

  /** @param {!PersonalizationHubBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }

  /** @override */
  openPersonalizationHub() {
    chrome.send('openPersonalizationHub');
  }
}
