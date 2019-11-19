// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for proxy.
 */
cca.proxy = cca.proxy || {};

(function() {
/* eslint-disable new-cap */

/** @throws {Error} */
function NOTIMPLEMENTED() {
  throw Error('Browser proxy method not implemented!');
}

/**
 * The WebUI implementation of the CCA's interaction with the browser.
 * @implements {cca.proxy.BrowserProxy}
 */
class WebUIBrowserProxy {
  /** @override */
  getVolumeList(callback) {
    NOTIMPLEMENTED();
  }

  /** @override */
  requestFileSystem(options, callback) {
    NOTIMPLEMENTED();
  }

  /** @override */
  localStorageGet(keys, callback) {
    let sanitizedKeys = [];
    if (typeof keys === 'string') {
      sanitizedKeys = [keys];
    } else if (Array.isArray(keys)) {
      sanitizedKeys = keys;
    } else if (keys !== null && typeof keys === 'object') {
      sanitizedKeys = Object.keys(keys);
    } else {
      throw new Error('WebUI localStorageGet() cannot be run with ' + keys);
    }

    let result = {};
    for (let key of sanitizedKeys) {
      let value = window.localStorage.getItem(key);
      if (value !== null) {
        value = JSON.parse(value);
      }
      result[key] = value === null ? {} : value;
    }

    callback(result);
  }

  /** @override */
  localStorageSet(items, callback) {
    for (let [key, val] of Object.entries(items)) {
      window.localStorage.setItem(key, JSON.stringify(val));
    }
    if (callback) {
      callback();
    }
  }

  /** @override */
  localStorageRemove(items, callback) {
    if (typeof items === 'string') {
      items = [items];
    }
    for (let key of items) {
      window.localStorage.removeItem(key);
    }
    if (callback) {
      callback();
    }
  }
}

/* eslint-enable new-cap */

/**
 * Namespace for browser functions.
 * @type {cca.proxy.BrowserProxy}
 */
cca.proxy.browserProxy = new WebUIBrowserProxy();
})();
