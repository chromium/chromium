// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppNotificationsHandler, AppNotificationsHandlerInterface} from '../../../mojom-webui/os_apps_page/app_notification_handler.mojom-webui.js';

/**
 * @type {?AppNotificationsHandlerInterface}
 */
let appNotificationProvider = null;

/**
 * @param {!AppNotificationsHandlerInterface} testProvider
 */
export function setAppNotificationProviderForTesting(testProvider) {
  appNotificationProvider = testProvider;
}

/**
 * @return {!AppNotificationsHandlerInterface}
 */
export function getAppNotificationProvider() {
  // For testing only.
  if (appNotificationProvider) {
    return appNotificationProvider;
  }
  appNotificationProvider = AppNotificationsHandler.getRemote();
  return appNotificationProvider;
}
