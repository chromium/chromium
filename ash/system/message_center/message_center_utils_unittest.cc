// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_utils.h"

#include "ash/media/media_notification_constants.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace message_center_utils {

namespace {

void AddNotification(const std::string& notification_id,
                     const std::string& app_id) {
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_BASE_FORMAT, notification_id,
          base::UTF8ToUTF16("test_title"), base::UTF8ToUTF16("test message"),
          gfx::Image(), /*display_source=*/base::string16(), GURL(),
          message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                     app_id),
          message_center::RichNotificationData(),
          new message_center::NotificationDelegate()));
}

}  // namespace

class MessageCenterUtilsTest : public AshTestBase,
                               public testing::WithParamInterface<bool> {
 public:
  MessageCenterUtilsTest() = default;
  MessageCenterUtilsTest(const MessageCenterUtilsTest&) = delete;
  MessageCenterUtilsTest& operator=(const MessageCenterUtilsTest&) = delete;
  ~MessageCenterUtilsTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    scoped_feature_list_.InitWithFeatureState(
        features::kMediaNotificationsCounter,
        IsMediaNotificationsCounterEnabled());
  }

  bool IsMediaNotificationsCounterEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MessageCenterUtilsTest,
    testing::Bool() /* IsMediaNotificationsCounterEnabled() */);

TEST_P(MessageCenterUtilsTest, TotalNotificationCount) {
  EXPECT_EQ(0u, GetNotificationCount());

  // VM camera/mic notifications are ignored by the counter.
  AddNotification("0", kVmCameraMicNotifierId);
  EXPECT_EQ(0u, GetNotificationCount());

  AddNotification("1", kMediaSessionNotifierId);
  // Counter should ignore media notifications when feature is enabled.
  size_t expected_count = IsMediaNotificationsCounterEnabled() ? 0u : 1u;
  EXPECT_EQ(expected_count, GetNotificationCount());
}

}  // namespace message_center_utils

}  // namespace ash
