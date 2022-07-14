// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
export class CursorAndTouchpadPageBrowserProxy {
  /**
   * Records the value of the show shelf navigation button.
   * @param {boolean} enabled
   */
  recordSelectedShowShelfNavigationButtonValue(enabled) {}
}

/** @type {?CursorAndTouchpadPageBrowserProxy} */
let instance = null;

/**
 * @implements {CursorAndTouchpadPageBrowserProxy}
 */
export class CursorAndTouchpadPageBrowserProxyImpl {
  /** @return {!CursorAndTouchpadPageBrowserProxy} */
  static getInstance() {
    return instance || (instance = new CursorAndTouchpadPageBrowserProxyImpl());
  }

  /** @param {!CursorAndTouchpadPageBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  recordSelectedShowShelfNavigationButtonValue(enabled) {
    chrome.send('recordSelectedShowShelfNavigationButtonValue', [enabled]);
  }
}
