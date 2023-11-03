// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class chimeBrowserProxy {
  /**
   * Initializes web contents in the WebUI handler.
   */
  initialize() {
    chrome.send('InitializeChimeHandler');
  }

  /**
   * Triggers adding the chime handler as a chime client.
   */
  SendAddChimeClient() {
    chrome.send('AddChimeClient');
  }

  /** @return {!chimeBrowserProxy} */
  static getInstance() {
    return instance || (instance = new chimeBrowserProxy());
  }
}

/** @type {?chimeBrowserProxy} */
let instance = null;
