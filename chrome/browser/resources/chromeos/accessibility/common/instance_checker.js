// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('InstanceChecker');

/**
 * Used to prevent multiple instances of the extension from running
 * simultaneously.
 */
const InstanceChecker = class {
  static closeExtraInstances() {
    // In 'split' manifest mode, the extension system runs two copies of the
    // extension. One in an incognito context; the other not. In guest mode, the
    // extension system runs only the extension in an incognito context. To
    // prevent doubling of this extension, only continue for one context.
    const manifest =
        /** @type {{incognito: (string|undefined)}} */ (
            chrome.runtime.getManifest());
    if (manifest.incognito === 'split' &&
        !chrome.extension.inIncognitoContext) {
      window.close();
    }
  }
};
