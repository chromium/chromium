// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface OsA11yPageBrowserProxy {
  /**
   * Requests whether screen reader state changed. Result
   * is returned by the 'screen-reader-state-changed' WebUI listener event.
   */
  a11yPageReady(): void;

  /**
   * Opens the a11y image labels modal dialog.
   */
  confirmA11yImageLabels(): void;
}

let instance: OsA11yPageBrowserProxy|null = null;

export class OsA11yPageBrowserProxyImpl implements OsA11yPageBrowserProxy {
  static getInstance(): OsA11yPageBrowserProxy {
    return instance || (instance = new OsA11yPageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: OsA11yPageBrowserProxy): void {
    instance = obj;
  }

  a11yPageReady(): void {
    chrome.send('a11yPageReady');
  }

  confirmA11yImageLabels(): void {
    chrome.send('confirmA11yImageLabels');
  }
}
