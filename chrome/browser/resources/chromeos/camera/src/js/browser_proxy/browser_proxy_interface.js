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

/**
 * The abstract interface for the CCA's interaction with the browser.
 * @interface
 */
cca.proxy.BrowserProxy = class {
  /** @param {function(!Array<!chrome.fileSystem.Volume>=)} callback */
  getVolumeList(callback) {}

  /**
   * @param {!chrome.fileSystem.RequestFileSystemOptions} options
   * @param {function(!FileSystem=)} callback
   */
  requestFileSystem(options, callback) {}

  /**
   * @param {(string|!Array<string>|!Object)} keys
   * @param {function(!Object)} callback
   */
  localStorageGet(keys, callback) {}

  /**
   * @param {!Object<string>} items
   * @param {function()=} callback
   */
  localStorageSet(items, callback) {}

  /**
   * @param {(string|!Array<string>)} items
   * @param {function()=} callback
   */
  localStorageRemove(items, callback) {}
};
