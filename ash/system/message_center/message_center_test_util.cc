// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_test_util.h"

#include "ui/message_center/public/cpp/notification.h"

namespace ash {

std::unique_ptr<message_center::Notification> CreateSimpleNotification(
    const std::string& id) {
  return std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, u"test_title",
      u"test message", ui::ImageModel(), std::u16string() /* display_source */,
      GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(),
      new message_center::NotificationDelegate());
}

}  // namespace ash
