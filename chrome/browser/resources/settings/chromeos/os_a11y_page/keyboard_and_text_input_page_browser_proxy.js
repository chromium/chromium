// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
export class KeyboardAndTextInputPageBrowserProxy {
  /**
   * Calls MaybeAddSodaInstallerObserver() and MaybeAddDictationLocales().
   */
  keyboardAndTextInputPageReady() {}
}

/** @type {?KeyboardAndTextInputPageBrowserProxy} */
let instance = null;

/**
 * @implements {KeyboardAndTextInputPageBrowserProxy}
 */
export class KeyboardAndTextInputPageBrowserProxyImpl {
  /** @return {!KeyboardAndTextInputPageBrowserProxy} */
  static getInstance() {
    return instance ||
        (instance = new KeyboardAndTextInputPageBrowserProxyImpl());
  }

  /** @param {!KeyboardAndTextInputPageBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  keyboardAndTextInputPageReady() {
    chrome.send('manageA11yPageReady');
  }
}
