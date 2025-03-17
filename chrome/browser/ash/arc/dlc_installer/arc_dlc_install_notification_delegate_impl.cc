// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/dlc_installer/arc_dlc_install_notification_delegate_impl.h"

#include <memory>

#include "base/check.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/message_center/public/cpp/notification.h"

namespace arc {

ArcDlcInstallNotificationManagerDelegateImpl::
    ArcDlcInstallNotificationManagerDelegateImpl(Profile* profile)
    : profile_(profile) {}

ArcDlcInstallNotificationManagerDelegateImpl::
    ~ArcDlcInstallNotificationManagerDelegateImpl() = default;

void ArcDlcInstallNotificationManagerDelegateImpl::DisplayNotification(
    const message_center::Notification& notification) {
  auto* notification_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  CHECK(notification_service);
  notification_service->Display(NotificationHandler::Type::TRANSIENT,
                                notification, /*metadata=*/nullptr);
}

}  // namespace arc
