// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let instance: ChimeBrowserProxy|null = null;

export class ChimeBrowserProxy {
  /**
   * Initializes web contents in the WebUI handler.
   */
  initialize() {
    chrome.send('InitializeChimeHandler');
  }

  /**
   * Triggers adding the chime handler as a chime client.
   */
  sendAddChimeClient() {
    chrome.send('AddChimeClient');
  }

  static getInstance(): ChimeBrowserProxy {
    return instance || (instance = new ChimeBrowserProxy());
  }
}
