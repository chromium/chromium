// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface OsResetBrowserProxy {
  /**
   * Initiates a factory reset and restarts.
   */
  requestFactoryResetRestart(): void;

  /**
   * A method to be called when the reset sanitize dialog is shown.
   */
  onShowSanitizeDialog(): void;

  /**
   * Sanitizes settings.
   */
  performSanitizeSettings(): void;
}

let instance: OsResetBrowserProxy|null = null;

export class OsResetBrowserProxyImpl implements OsResetBrowserProxy {
  static getInstance(): OsResetBrowserProxy {
    return instance || (instance = new OsResetBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: OsResetBrowserProxy): void {
    instance = obj;
  }

  requestFactoryResetRestart(): void {
    chrome.send('requestFactoryResetRestart');
  }

  onShowSanitizeDialog(): void {
    chrome.send('onShowSanitizeDialog');
  }

  performSanitizeSettings(): void {
    chrome.send('performSanitizeSettings');
  }
}
