// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {NotificationsInternalsBrowserProxy} from './notifications_internals_browser_proxy.js';
import {NotificationsInternalsBrowserProxyImpl} from './notifications_internals_browser_proxy.js';

function initialize() {
  const browserProxy: NotificationsInternalsBrowserProxy =
      NotificationsInternalsBrowserProxyImpl.getInstance();

  // Register all event listeners.
  getRequiredElement('schedule-notification').onclick = function() {
    browserProxy.scheduleNotification(
        getRequiredElement<HTMLInputElement>('notification-scheduler-url')
            .value,
        getRequiredElement<HTMLInputElement>('notification-scheduler-title')
            .value,
        getRequiredElement<HTMLInputElement>('notification-scheduler-message')
            .value);
  };
}

document.addEventListener('DOMContentLoaded', initialize);
