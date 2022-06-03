// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
class LocalStorageProxy {
  /**
   * @param {string} key
   * @return {?string}
   */
  getItem(key) {}

  /**
   * @param {string} key
   * @param {string} value
   */
  setItem(key, value) {}
}

/** @implements {LocalStorageProxy} */
export class LocalStorageProxyImpl {
  /** @override */
  getItem(key) {
    return window.localStorage.getItem(key);
  }

  /** @override */
  setItem(key, value) {
    window.localStorage.setItem(key, value);
  }

  /** @return {!LocalStorageProxy} */
  static getInstance() {
    return instance || (instance = new LocalStorageProxyImpl());
  }
}

/** @type {?LocalStorageProxy} */
let instance = null;
