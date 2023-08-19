// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppNotificationsHandler, AppNotificationsHandlerInterface} from '../../mojom-webui/app_notification_handler.mojom-webui.js';

let appNotificationProvider: AppNotificationsHandlerInterface|null = null;

export function setAppNotificationProviderForTesting(
    testProvider: AppNotificationsHandlerInterface): void {
  appNotificationProvider = testProvider;
}

export function getAppNotificationProvider(): AppNotificationsHandlerInterface {
  // For testing only.
  if (appNotificationProvider) {
    return appNotificationProvider;
  }
  appNotificationProvider = AppNotificationsHandler.getRemote();
  return appNotificationProvider;
}
