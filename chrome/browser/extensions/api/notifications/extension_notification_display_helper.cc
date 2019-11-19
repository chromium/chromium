// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notifications/extension_notification_display_helper.h"

#include <algorithm>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

namespace extensions {

ExtensionNotificationDisplayHelper::ExtensionNotificationDisplayHelper(
    Profile* profile)
    : profile_(profile) {}

ExtensionNotificationDisplayHelper::~ExtensionNotificationDisplayHelper() {}

void ExtensionNotificationDisplayHelper::Display(
    const message_center::Notification& notification) {
  // Remove the previous version of this notification if the |notification| is
  // updating another notification.
  EraseDataForNotificationId(notification.id());

  notifications_.push_back(
      std::make_unique<message_center::Notification>(notification));

  GetDisplayService()->Display(NotificationHandler::Type::EXTENSION,
                               notification, /*metadata=*/nullptr);
}

message_center::Notification*
ExtensionNotificationDisplayHelper::GetByNotificationId(
    const std::string& notification_id) {
  for (const auto& notification : notifications_) {
    if (notification->id() == notification_id)
      return notification.get();
  }

  return nullptr;
}

std::set<std::string>
ExtensionNotificationDisplayHelper::GetNotificationIdsForExtension(
    const GURL& extension_origin) const {
  std::set<std::string> notification_ids;
  for (const auto& notification : notifications_) {
    if (notification->origin_url() == extension_origin)
      notification_ids.insert(notification->id());
  }

  return notification_ids;
}

bool ExtensionNotificationDisplayHelper::EraseDataForNotificationId(
    const std::string& notification_id) {
  auto iter = std::find_if(
      notifications_.begin(), notifications_.end(),
      [notification_id](
          const std::unique_ptr<message_center::Notification>& notification) {
        return notification->id() == notification_id;
      });

  if (iter == notifications_.end())
    return false;

  notifications_.erase(iter);
  return true;
}

bool ExtensionNotificationDisplayHelper::Close(
    const std::string& notification_id) {
  if (!EraseDataForNotificationId(notification_id))
    return false;

  GetDisplayService()->Close(NotificationHandler::Type::EXTENSION,
                             notification_id);
  return true;
}

void ExtensionNotificationDisplayHelper::Shutdown() {
  // Do not call GetDisplayService()->Close() here. The Shutdown method is
  // called upon profile destruction and closing a notification requires a
  // profile, as it may dispatch an event to the extension.

  notifications_.clear();
}

NotificationDisplayService*
ExtensionNotificationDisplayHelper::GetDisplayService() {
  return NotificationDisplayServiceFactory::GetForProfile(profile_);
}

}  // namespace extensions
