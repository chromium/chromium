// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/do_not_disturb_notification_controller.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

using message_center::MessageCenter;

std::u16string GetDoNotDisturbDescription() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_DO_NOT_DISTURB_NOTIFICATION_DESCRIPTION);
}

message_center::Notification* GetDoNotDisturbNotification() {
  return MessageCenter::Get()->FindNotificationById(
      DoNotDisturbNotificationController::kDoNotDisturbNotificationId);
}

}  // namespace

class DoNotDisturbNotificationControllerTest : public AshTestBase {
 public:
  DoNotDisturbNotificationControllerTest() = default;
  DoNotDisturbNotificationControllerTest(
      const DoNotDisturbNotificationControllerTest&) = delete;
  DoNotDisturbNotificationControllerTest& operator=(
      const DoNotDisturbNotificationControllerTest&) = delete;
  ~DoNotDisturbNotificationControllerTest() override = default;
};

// Tests that enabling/disabling Do not disturb mode adds/removes the Do not
// disturb notification.
TEST_F(DoNotDisturbNotificationControllerTest, AddRemoveNotification) {
  auto* message_center = MessageCenter::Get();
  ASSERT_FALSE(GetDoNotDisturbNotification());

  // Turn on Do not disturb mode.
  message_center->SetQuietMode(true);
  auto* notification = GetDoNotDisturbNotification();
    EXPECT_TRUE(notification);
    EXPECT_EQ(notification->message(), GetDoNotDisturbDescription());

  // Turn off Do not disturb mode.
  message_center->SetQuietMode(false);
  EXPECT_FALSE(GetDoNotDisturbNotification());
}

// Tests that clicking the notification's "Turn off" button turns off Do not
// disturb mode and dismisses the Do not disturb notification.
TEST_F(DoNotDisturbNotificationControllerTest,
       NotificationButtonTurnsOffDoNotDisturbMode) {
  // Show the notification by turning on Do not disturb mode.
  auto* message_center = MessageCenter::Get();
  message_center->SetQuietMode(true);
  auto* notification = GetDoNotDisturbNotification();
  ASSERT_TRUE(notification);
  ASSERT_EQ(notification->message(), GetDoNotDisturbDescription());

  // Simulate a click on the notification's "Turn off" button.
  notification->delegate()->Click(0, std::nullopt);
  EXPECT_FALSE(GetDoNotDisturbNotification());
  EXPECT_FALSE(message_center->IsQuietMode());
}

class DoNotDisturbNotificationControllerWithFocusModeTest : public AshTestBase {
 public:
  DoNotDisturbNotificationControllerWithFocusModeTest()
      : scoped_feature_list_(features::kFocusMode) {}
  ~DoNotDisturbNotificationControllerWithFocusModeTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests if the correct notification id shows up when the system DND is on
// before starting a focus session.
TEST_F(DoNotDisturbNotificationControllerWithFocusModeTest,
       CheckNotificationTypesIfSystemDNDIsOnInitially) {
  auto* message_center = MessageCenter::Get();
  auto* focus_mode_controller = FocusModeController::Get();

  // Turn on system DND mode before starting a session.
  message_center->SetQuietMode(true);
  auto* notification = GetDoNotDisturbNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(notification->message(), GetDoNotDisturbDescription());

  // Start a focus session where `turn_on_do_not_disturb` is defaulted to
  // `true`.
  EXPECT_TRUE(focus_mode_controller->turn_on_do_not_disturb());
  focus_mode_controller->ToggleFocusMode();
  EXPECT_TRUE(focus_mode_controller->in_focus_session());
  notification = GetDoNotDisturbNotification();
  EXPECT_TRUE(notification);
  EXPECT_EQ(notification->message(), GetDoNotDisturbDescription());

  // Check the notification creation during the focus session. First, disable
  // the DND mode in the focus session.
  notification->delegate()->Click(0, std::nullopt);
  EXPECT_FALSE(GetDoNotDisturbNotification());
  EXPECT_FALSE(message_center->IsQuietMode());
  // Second, enable the DND mode.
  message_center->SetQuietMode(true);
  notification = GetDoNotDisturbNotification();
  EXPECT_TRUE(notification);
  EXPECT_EQ(notification->message(), GetDoNotDisturbDescription());

  // End the focus session, and the system DND state will be restored to `true`.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_FALSE(focus_mode_controller->in_focus_session());
  notification = GetDoNotDisturbNotification();
  EXPECT_TRUE(notification);
  EXPECT_EQ(notification->message(), GetDoNotDisturbDescription());
}

// Tests if the correct notification id shows up when the system DND is off
// before starting a focus session.
TEST_F(DoNotDisturbNotificationControllerWithFocusModeTest,
       CheckNotificationTypesIfSystemDNDIsOffInitially) {
  auto* focus_mode_controller = FocusModeController::Get();
  auto* message_center = MessageCenter::Get();

  // The system DND is off before starting a focus session.
  ASSERT_FALSE(GetDoNotDisturbNotification());
  EXPECT_FALSE(message_center->IsQuietMode());
  EXPECT_NE(message_center::QuietModeSourceType::kFocusMode,
            message_center->GetLastQuietModeChangeSourceType());

  // Start a focus session where `turn_on_do_not_disturb` is defaulted to
  // `true`.
  EXPECT_TRUE(focus_mode_controller->turn_on_do_not_disturb());
  focus_mode_controller->ToggleFocusMode();
  EXPECT_TRUE(focus_mode_controller->in_focus_session());
  auto* notification = GetDoNotDisturbNotification();
  EXPECT_TRUE(notification);
  const base::Time end_time = focus_mode_controller->GetActualEndTime();
  EXPECT_EQ(
      notification->message(),
      focus_mode_util::GetNotificationDescriptionForFocusSession(end_time));
  // Check that quiet mode is active, and it was triggered by focus mode.
  EXPECT_TRUE(message_center->IsQuietMode());
  EXPECT_EQ(message_center::QuietModeSourceType::kFocusMode,
            message_center->GetLastQuietModeChangeSourceType());

  // End the focus session, and the system DND state will be restored to
  // `false`.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_FALSE(focus_mode_controller->in_focus_session());
  EXPECT_FALSE(GetDoNotDisturbNotification());
  // Check that quiet mode is no longer active, and it was triggered by focus
  // mode.
  EXPECT_FALSE(message_center->IsQuietMode());
  EXPECT_EQ(message_center::QuietModeSourceType::kFocusMode,
            message_center->GetLastQuietModeChangeSourceType());
}

// Tests that if the focus session is extended by 10 minutes; correspondingly,
// the notification will show the updated end time after adding the 10 minutes.
TEST_F(DoNotDisturbNotificationControllerWithFocusModeTest,
       CheckNotificationTextAfterExtendingFocusSession) {
  auto* focus_mode_controller = FocusModeController::Get();

  // The system DND is off before starting a focus session.
  ASSERT_FALSE(GetDoNotDisturbNotification());

  // Start a focus session where `turn_on_do_not_disturb` is defaulted to
  // `true`.
  EXPECT_TRUE(focus_mode_controller->turn_on_do_not_disturb());
  focus_mode_controller->ToggleFocusMode();
  EXPECT_TRUE(focus_mode_controller->in_focus_session());
  const base::Time end_time1 = focus_mode_controller->GetActualEndTime();

  auto* notification = GetDoNotDisturbNotification();
  EXPECT_TRUE(notification);
  EXPECT_EQ(
      notification->message(),
      focus_mode_util::GetNotificationDescriptionForFocusSession(end_time1));

  // Extend the focus duration.
  focus_mode_controller->ExtendSessionDuration();
  const base::Time end_time2 = focus_mode_controller->GetActualEndTime();
  EXPECT_EQ(end_time2 - end_time1, base::Minutes(10));

  notification = GetDoNotDisturbNotification();
  EXPECT_TRUE(notification);
  EXPECT_EQ(
      notification->message(),
      focus_mode_util::GetNotificationDescriptionForFocusSession(end_time2));

  // End the focus session, and the system DND state will be restored to
  // `false`.
  focus_mode_controller->ToggleFocusMode();
  EXPECT_FALSE(focus_mode_controller->in_focus_session());
  EXPECT_FALSE(GetDoNotDisturbNotification());
}

}  // namespace ash
