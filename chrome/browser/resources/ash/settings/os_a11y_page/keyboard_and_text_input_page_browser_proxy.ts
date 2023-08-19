// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface KeyboardAndTextInputPageBrowserProxy {
  /**
   * Calls MaybeAddSodaInstallerObserver() and MaybeAddDictationLocales().
   */
  keyboardAndTextInputPageReady(): void;
}

let instance: KeyboardAndTextInputPageBrowserProxy|null = null;

export class KeyboardAndTextInputPageBrowserProxyImpl implements
    KeyboardAndTextInputPageBrowserProxy {
  static getInstance(): KeyboardAndTextInputPageBrowserProxy {
    return instance ||
        (instance = new KeyboardAndTextInputPageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: KeyboardAndTextInputPageBrowserProxy):
      void {
    instance = obj;
  }

  keyboardAndTextInputPageReady(): void {
    chrome.send('manageA11yPageReady');
  }
}
