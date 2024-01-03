// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_MANAGER_DESKTOP_IMPL_H_
#define CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_MANAGER_DESKTOP_IMPL_H_

#include "components/push_notification/push_notification_client_manager.h"

namespace push_notification {

class PushNotificationClientManagerDesktopImpl
    : public PushNotificationClientManager {
 public:
  PushNotificationClientManagerDesktopImpl();
  PushNotificationClientManagerDesktopImpl(
      const PushNotificationClientManagerDesktopImpl&) = delete;
  PushNotificationClientManagerDesktopImpl& operator=(
      const PushNotificationClientManagerDesktopImpl&) = delete;
  ~PushNotificationClientManagerDesktopImpl() override;

  // PushNotificationClientManager:
  void NotifyPushNotificationClientOfMessage(
      PushNotificationMessage message) override;
};

}  // namespace push_notification

#endif  // CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_MANAGER_DESKTOP_IMPL_H_
