// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface CursorAndTouchpadPageBrowserProxy {
  /**
   * Records the value of the show shelf navigation button.
   */
  recordSelectedShowShelfNavigationButtonValue(enabled: boolean): void;
}

let instance: CursorAndTouchpadPageBrowserProxy|null = null;

export class CursorAndTouchpadPageBrowserProxyImpl implements
    CursorAndTouchpadPageBrowserProxy {
  static getInstance(): CursorAndTouchpadPageBrowserProxy {
    return instance || (instance = new CursorAndTouchpadPageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: CursorAndTouchpadPageBrowserProxy): void {
    instance = obj;
  }

  recordSelectedShowShelfNavigationButtonValue(enabled: boolean): void {
    chrome.send('recordSelectedShowShelfNavigationButtonValue', [enabled]);
  }
}
