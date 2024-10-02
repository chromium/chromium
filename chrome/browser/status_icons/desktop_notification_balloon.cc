// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/desktop_notification_balloon.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace {

// Prefix added to the notification ids.
const char kDesktopNotificationPrefix[] = "desktop_notification_balloon.";

}  // anonymous namespace

int DesktopNotificationBalloon::id_count_ = 1;

DesktopNotificationBalloon::DesktopNotificationBalloon() {}

DesktopNotificationBalloon::~DesktopNotificationBalloon() {}

void DesktopNotificationBalloon::DisplayBalloon(
    const ui::ImageModel& icon,
    const std::u16string& title,
    const std::u16string& contents,
    const message_center::NotifierId& notifier_id) {
  // Allowing IO access is required here to cover the corner case where
  // there is no last used profile and the default one is loaded.
  // IO access won't be required for normal uses.
  Profile* profile;
  {
    base::ScopedAllowBlocking allow_blocking;
    profile = ProfileManager::GetLastUsedProfile();
  }

  const std::string notification_id =
      kDesktopNotificationPrefix + base::NumberToString(id_count_++);
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title,
      contents, icon, std::u16string(), GURL(), notifier_id, {},
      new message_center::NotificationDelegate());

  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, notification, /*metadata=*/nullptr);
}
