// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
export class PersonalizationHubBrowserProxy {
  openPersonalizationHub() {}
}

/**
 * @implements {PersonalizationHubBrowserProxy}
 */
export class PersonalizationHubBrowserProxyImpl {
  /** @override */
  openPersonalizationHub() {
    chrome.send('openPersonalizationHub');
  }

  /** @return {!PersonalizationHubBrowserProxy} */
  static getInstance() {
    return instance || (instance = new PersonalizationHubBrowserProxyImpl());
  }

  /** @param {!PersonalizationHubBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?PersonalizationHubBrowserProxy} */
let instance = null;
