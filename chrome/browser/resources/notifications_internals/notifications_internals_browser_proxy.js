// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('notificationsInternals', function() {
  /** @interface */
  class NotificationsInternalsBrowserProxy {
    /**
     * Schedules a notification through notification schedule service.
     * @param {string} url URL to open after clicking the notification.
     * @param {string} title Title of the notification.
     * @param {string} message Message of the notification.
     */
    scheduleNotification(url, title, message) {}
  }

  /**
   * @implements {notificationsInternals.NotificationsInternalsBrowserProxy}
   */
  class NotificationsInternalsBrowserProxyImpl {
    /** @override */
    scheduleNotification(url, title, message) {
      return cr.sendWithPromise('scheduleNotification', url, title, message);
    }
  }

  cr.addSingletonGetter(NotificationsInternalsBrowserProxyImpl);

  return {
    NotificationsInternalsBrowserProxy: NotificationsInternalsBrowserProxy,
    NotificationsInternalsBrowserProxyImpl:
        NotificationsInternalsBrowserProxyImpl
  };
});
