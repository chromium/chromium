// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface OsA11yPageBrowserProxy {
  /**
   * Opens the a11y image labels modal dialog.
   */
  confirmA11yImageLabels(): void;

  /**
   * Requests the current state of screen reader. Result is returned with a
   * Promise.
   */
  getScreenReaderState(): Promise<boolean>;
}

let instance: OsA11yPageBrowserProxy|null = null;

export class OsA11yPageBrowserProxyImpl implements OsA11yPageBrowserProxy {
  static getInstance(): OsA11yPageBrowserProxy {
    return instance || (instance = new OsA11yPageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: OsA11yPageBrowserProxy): void {
    instance = obj;
  }

  confirmA11yImageLabels(): void {
    chrome.send('confirmA11yImageLabels');
  }

  getScreenReaderState(): Promise<boolean> {
    return sendWithPromise('getScreenReaderState');
  }
}
