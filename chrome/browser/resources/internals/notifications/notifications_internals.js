// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';

import {NotificationsInternalsBrowserProxy, NotificationsInternalsBrowserProxyImpl} from './notifications_internals_browser_proxy.js';

function initialize() {
  /** @type {!NotificationsInternalsBrowserProxy} */
  const browserProxy = NotificationsInternalsBrowserProxyImpl.getInstance();

  // Register all event listeners.
  $('schedule-notification').onclick = function() {
    browserProxy.scheduleNotification(
        $('notification-scheduler-url').value,
        $('notification-scheduler-title').value,
        $('notification-scheduler-message').value);
  };
}

document.addEventListener('DOMContentLoaded', initialize);
