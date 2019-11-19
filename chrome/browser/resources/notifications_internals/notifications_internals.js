// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('notificationsInternals', function() {
  'use strict';

  const browserProxy =
      notificationsInternals.NotificationsInternalsBrowserProxyImpl
          .getInstance();

  function initialize() {
    // Register all event listeners.
    $('schedule-notification').onclick = function() {
      browserProxy.scheduleNotification(
          $('notification-scheduler-url').value,
          $('notification-scheduler-title').value,
          $('notification-scheduler-message').value);
    };
  }

  return {
    initialize: initialize,
  };
});

document.addEventListener(
    'DOMContentLoaded', notificationsInternals.initialize);
