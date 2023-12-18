// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class PushNotificationBrowserProxy {
  /**
   * Initializes web contents in the WebUI handler.
   */
  initialize() {
    chrome.send('InitializePushNotificationHandler');
  }

  /**
   * Triggers adding the push notification handler as a push notification
   * client.
   */
  SendAddPushNotificationClient() {
    chrome.send('AddPushNotificationClient');
  }

  /** @return {!PushNotificationBrowserProxy} */
  static getInstance() {
    return instance || (instance = new PushNotificationBrowserProxy());
  }
}

/** @type {?PushNotificationBrowserProxy} */
let instance = null;
