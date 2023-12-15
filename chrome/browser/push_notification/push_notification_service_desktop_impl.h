// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_DESKTOP_IMPL_H_
#define CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_DESKTOP_IMPL_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/push_notification/push_notification_service.h"

namespace push_notification {

class PushNotificationServiceDesktopImpl : public PushNotificationService,
                                           public KeyedService {
 public:
  PushNotificationServiceDesktopImpl();
  PushNotificationServiceDesktopImpl(
      const PushNotificationServiceDesktopImpl&) = delete;
  PushNotificationServiceDesktopImpl& operator=(
      const PushNotificationServiceDesktopImpl&) = delete;
  ~PushNotificationServiceDesktopImpl() override;

 private:
  // KeyedService:
  void Shutdown() override;
};

}  // namespace push_notification

#endif  // CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_DESKTOP_IMPL_H_
