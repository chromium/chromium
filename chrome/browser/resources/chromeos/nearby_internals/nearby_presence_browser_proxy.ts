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
  sendStartScan() {
    chrome.send('StartPresenceScan');
  }

  /**
   * Triggers NearbyPresenceService to stop a scan if a scan is currently
   * running.
   */
  sendStopScan() {
    chrome.send('StopPresenceScan');
  }

  /**
   * Tells NearbyPresenceService to sync Presence credentials.
   */
  sendSyncCredentials() {
    chrome.send('SyncPresenceCredentials');
  }

  /**
   * Tells NearbyPresenceService to start a first time flow for retreiving
   * credentials.
   */
  sendFirstTimeFlow() {
    chrome.send('FirstTimePresenceFlow');
  }

  connectToPresenceDevice(endpointId: string) {
    chrome.send('ConnectToPresenceDevice', [endpointId]);
  }

  /**
   * Triggers sending a PushNotification message for the 'NearbyPresenceService'
   * to reload credentials.
   */
  sendUpdateCredentialsPushNotificationMessage() {
    chrome.send('SendUpdateCredentialsMessage');
  }

  static getInstance(): NearbyPresenceBrowserProxy {
    return instance || (instance = new NearbyPresenceBrowserProxy());
  }
}

let instance: NearbyPresenceBrowserProxy|null = null;
