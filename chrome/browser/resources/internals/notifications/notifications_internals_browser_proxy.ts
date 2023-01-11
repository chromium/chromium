// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let instance: NotificationsInternalsBrowserProxy|null = null;

export interface NotificationsInternalsBrowserProxy {
  /**
   * Schedules a notification through notification schedule service.
   * @param url URL to open after clicking the notification.
   * @param title Title of the notification.
   * @param message Message of the notification.
   */
  scheduleNotification(url: string, title: string, message: string): void;
}

export class NotificationsInternalsBrowserProxyImpl implements
    NotificationsInternalsBrowserProxy {
  scheduleNotification(url: string, title: string, message: string) {
    chrome.send('scheduleNotification', [url, title, message]);
  }

  static getInstance(): NotificationsInternalsBrowserProxy {
    return instance ||
        (instance = new NotificationsInternalsBrowserProxyImpl());
  }
}
