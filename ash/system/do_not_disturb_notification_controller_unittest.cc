// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/do_not_disturb_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

using message_center::MessageCenter;

}  // namespace

class DoNotDisturbNotificationControllerTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  DoNotDisturbNotificationControllerTest() = default;
  DoNotDisturbNotificationControllerTest(
      const DoNotDisturbNotificationControllerTest&) = delete;
  DoNotDisturbNotificationControllerTest& operator=(
      const DoNotDisturbNotificationControllerTest&) = delete;
  ~DoNotDisturbNotificationControllerTest() override = default;

  void SetUp() override {
    if (IsQsRevampEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(features::kQsRevamp);
    }
    AshTestBase::SetUp();
  }

  bool IsQsRevampEnabled() { return GetParam(); }

  bool IsDoNotDisturbNotificationPresent() {
    return MessageCenter::Get()->FindNotificationById(
        DoNotDisturbNotificationController::kDoNotDisturbNotificationId);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         DoNotDisturbNotificationControllerTest,
                         testing::Bool() /* IsQsRevampEnabled() */);

// Tests that enabling/disabling Do not disturb mode adds/removes the Do not
// disturb notification.
TEST_P(DoNotDisturbNotificationControllerTest, AddRemoveNotification) {
  auto* message_center = MessageCenter::Get();
  ASSERT_FALSE(IsDoNotDisturbNotificationPresent());

  // Turn on Do not disturb mode.
  message_center->SetQuietMode(true);
  if (IsQsRevampEnabled()) {
    EXPECT_TRUE(IsDoNotDisturbNotificationPresent());
  } else {
    EXPECT_FALSE(IsDoNotDisturbNotificationPresent());
  }

  // Turn off Do not disturb mode.
  message_center->SetQuietMode(false);
  EXPECT_FALSE(IsDoNotDisturbNotificationPresent());
}

// Tests that clicking the notification's "Turn off" button turns off Do not
// disturb mode and dismisses the Do not disturb notification.
TEST_P(DoNotDisturbNotificationControllerTest,
       NotificationButtonTurnsOffDoNotDisturbMode) {
  if (!IsQsRevampEnabled()) {
    // The notification only appears when QsRevamp is enabled.
    return;
  }

  // Show the notification by turning on Do not disturb mode.
  auto* message_center = MessageCenter::Get();
  message_center->SetQuietMode(true);
  ASSERT_TRUE(IsDoNotDisturbNotificationPresent());

  // Simulate a click on the notification's "Turn off" button.
  auto* notification = message_center->FindNotificationById(
      DoNotDisturbNotificationController::kDoNotDisturbNotificationId);
  notification->delegate()->Click(0, absl::nullopt);
  EXPECT_FALSE(IsDoNotDisturbNotificationPresent());
  EXPECT_FALSE(message_center->IsQuietMode());
}

}  // namespace ash
