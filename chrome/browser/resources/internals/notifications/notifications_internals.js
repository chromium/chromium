// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NotificationsInternalsBrowserProxy, NotificationsInternalsBrowserProxyImpl} from './notifications_internals_browser_proxy.js';

function initialize() {
  /** @type {!NotificationsInternalsBrowserProxy} */
  const browserProxy = NotificationsInternalsBrowserProxyImpl.getInstance();

  // Register all event listeners.
  document.body.querySelector('#schedule-notification').onclick = function() {
    browserProxy.scheduleNotification(
        document.body.querySelector('#notification-scheduler-url').value,
        document.body.querySelector('#notification-scheduler-title').value,
        document.body.querySelector('#notification-scheduler-message').value);
  };
}

document.addEventListener('DOMContentLoaded', initialize);
