// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let instance: PushNotificationBrowserProxy|null = null;

export class PushNotificationBrowserProxy {
  /**
   * Initializes web contents in the WebUI handler.
   */
  initialize() {
    chrome.send('InitializePushNotificationHandler');
  }

  /**
   * Triggers adding the push notification handler as a PushNotificationClient.
   */
  sendAddPushNotificationClient() {
    chrome.send('AddPushNotificationClient');
  }

  static getInstance(): PushNotificationBrowserProxy {
    return instance || (instance = new PushNotificationBrowserProxy());
  }
}
