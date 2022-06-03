// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import '/os_apps_page/app_notification_handler.mojom-lite.js';

/**
 * @type {
 *    ?chromeos.settings.appNotification.mojom.AppNotificationsHandlerInterface
 * }
 */
let appNotificationProvider = null;

/**
 * @param {
 *    !chromeos.settings.appNotification.mojom.AppNotificationsHandlerInterface
 * } testProvider
 */
export function setAppNotificationProviderForTesting(testProvider) {
  appNotificationProvider = testProvider;
}

/**
 * @return {
 *    !chromeos.settings.appNotification.mojom.AppNotificationsHandlerInterface
 * }
 */
export function getAppNotificationProvider() {
  // For testing only.
  if (appNotificationProvider) {
    return appNotificationProvider;
  }
  appNotificationProvider = chromeos.settings.appNotification.mojom
                                .AppNotificationsHandler.getRemote();
  return appNotificationProvider;
}