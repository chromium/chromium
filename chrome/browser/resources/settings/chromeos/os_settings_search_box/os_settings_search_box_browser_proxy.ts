// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the os settings search section to
 * interact with the browser.
 */

export interface OsSettingsSearchBoxBrowserProxy {
  // <if expr="_google_chrome">
  /**
   * Opens the feedback dialog.
   */
  openSearchFeedbackDialog(descriptionTemplate: string): void;
  // </if>
}

let instance: OsSettingsSearchBoxBrowserProxy|null = null;

export class OsSettingsSearchBoxBrowserProxyImpl implements
    OsSettingsSearchBoxBrowserProxy {
  static getInstance(): OsSettingsSearchBoxBrowserProxy {
    return instance || (instance = new OsSettingsSearchBoxBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: OsSettingsSearchBoxBrowserProxy): void {
    instance = obj;
  }

  // <if expr="_google_chrome">
  openSearchFeedbackDialog(descriptionTemplate: string): void {
    // pass the search query as the value for the feedback dialog
    // description_template
    chrome.send('openSearchFeedbackDialog', [descriptionTemplate]);
  }
  // </if>
}
