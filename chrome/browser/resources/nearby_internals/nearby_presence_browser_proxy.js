// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class NearbyPresenceBrowserProxy {
  /**
   * Initializes web contents in the WebUI handler.
   */
  initialize() {
    chrome.send('InitializePresenceHandler');
  }

  /**
   * Triggers NearbyPresenceService to start a scan.
   */
  SendStartScan() {
    chrome.send('StartPresenceScan');
  }

  /**
   * Triggers NearbyPresenceService to stop a scan if a scan is currently
   * running.
   */
  SendStopScan() {
    chrome.send('StopPresenceScan');
  }

  /**
   * Tells NearbyPresenceService to sync Presence credentials.
   */
  SendSyncCredentials() {
    chrome.send('SyncPresenceCredentials');
  }

  /**
   * Tells NearbyPresenceService to start a first time flow for retreiving
   * credentials.
   */
  SendFirstTimeFlow() {
    chrome.send('FirstTimePresenceFlow');
  }

  ConnectToPresenceDevice(endpointId) {
    chrome.send('ConnectToPresenceDevice', [endpointId]);
  }

  /** @return {!NearbyPresenceBrowserProxy} */
  static getInstance() {
    return instance || (instance = new NearbyPresenceBrowserProxy());
  }
}

/** @type {?NearbyPresenceBrowserProxy} */
let instance = null;
