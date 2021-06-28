// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/system_notification_manager.h"

namespace file_manager {

SystemNotificationManager::SystemNotificationManager(Profile* profile)
    : profile_(profile) {}

SystemNotificationManager::~SystemNotificationManager() = default;

bool SystemNotificationManager::DoFilesSwaWindowsExist() {
  return false;
}

void SystemNotificationManager::HandleDeviceEvent(
    const file_manager_private::DeviceEvent& event) {
  auto notification = CreateNotification(kSWAnotification, u"SWA", u"From C++");
  GetNotificationDisplayService()->Display(NotificationHandler::Type::TRANSIENT,
                                           *notification,
                                           /*metadata=*/nullptr);
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::CreateNotification(std::string notification_id,
                                              const std::u16string& title,
                                              const std::u16string& message) {
  return ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title, message,
      std::u16string(), GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(),
      new message_center::HandleNotificationClickDelegate(base::BindRepeating(
          &SystemNotificationManager::Dismiss, weak_ptr_factory_.GetWeakPtr())),
      kNotificationGoogleIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

void SystemNotificationManager::Dismiss() {
  SystemNotificationHelper::GetInstance()->Close(kSWAnotification);
}

NotificationDisplayService*
SystemNotificationManager::GetNotificationDisplayService() {
  return NotificationDisplayServiceFactory::GetForProfile(profile_);
}

}  // namespace file_manager
