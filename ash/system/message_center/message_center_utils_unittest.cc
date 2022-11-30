// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_utils.h"

#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace message_center_utils {

namespace {

void AddNotification(const std::string& notification_id,
                     const std::string& app_id) {
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          u"test_title", u"test message", ui::ImageModel(),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                     app_id),
          message_center::RichNotificationData(),
          new message_center::NotificationDelegate()));
}

}  // namespace

using MessageCenterUtilsTest = AshTestBase;

TEST_F(MessageCenterUtilsTest, TotalNotificationCount) {
  EXPECT_EQ(0u, GetNotificationCount());

  // VM camera/mic notifications are ignored by the counter.
  AddNotification("0", kVmCameraMicNotifierId);
  EXPECT_EQ(0u, GetNotificationCount());
}

}  // namespace message_center_utils

}  // namespace ash
