// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class NotificationsInternalsBrowserProxy {
  /**
   * Schedules a notification through notification schedule service.
   * @param {string} url URL to open after clicking the notification.
   * @param {string} title Title of the notification.
   * @param {string} message Message of the notification.
   */
  scheduleNotification(url, title, message) {}
}

/**
 * @implements {NotificationsInternalsBrowserProxy}
 */
export class NotificationsInternalsBrowserProxyImpl {
  /** @override */
  scheduleNotification(url, title, message) {
    chrome.send('scheduleNotification', [url, title, message]);
  }
}

addSingletonGetter(NotificationsInternalsBrowserProxyImpl);
