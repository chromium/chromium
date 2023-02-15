// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/screen_security_controller.h"

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "ui/color/color_id.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

message_center::Notification* FindNotification(const std::string& id) {
  return message_center::MessageCenter::Get()->FindVisibleNotificationById(id);
}

// Check the visibility of privacy indicators in all displays.
void ExpectPrivacyIndicatorsVisible(bool visible) {
  for (ash::RootWindowController* root_window_controller :
       ash::Shell::Get()->GetAllRootWindowControllers()) {
    EXPECT_EQ(root_window_controller->GetStatusAreaWidget()
                  ->unified_system_tray()
                  ->privacy_indicators_view()
                  ->GetVisible(),
              visible);
  }
}

}  // namespace

class ScreenSecurityControllerTest : public AshTestBase,
                                     public testing::WithParamInterface<bool> {
 public:
  ScreenSecurityControllerTest() = default;
  ScreenSecurityControllerTest(const ScreenSecurityControllerTest&) = delete;
  ScreenSecurityControllerTest& operator=(const ScreenSecurityControllerTest&) =
      delete;
  ~ScreenSecurityControllerTest() override = default;

  // AppAccessNotifierBaseTest:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        features::kPrivacyIndicators, IsPrivacyIndicatorsFeatureEnabled());
    AshTestBase::SetUp();
  }

  bool IsPrivacyIndicatorsFeatureEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ScreenSecurityControllerTest,
    /*IsPrivacyIndicatorsFeatureEnabled()=*/::testing::Bool());

TEST_P(ScreenSecurityControllerTest, ShowScreenCaptureNotification) {
  Shell::Get()->system_tray_notifier()->NotifyScreenCaptureStart(
      base::DoNothing(), base::RepeatingClosure(), std::u16string());
  EXPECT_TRUE(FindNotification(kScreenCaptureNotificationId));

  Shell::Get()->system_tray_notifier()->NotifyScreenCaptureStop();

  EXPECT_FALSE(FindNotification(kScreenCaptureNotificationId));
}

TEST_P(ScreenSecurityControllerTest, ShowScreenShareNotification) {
  Shell::Get()->system_tray_notifier()->NotifyScreenShareStart(
      base::DoNothing(), std::u16string());

  EXPECT_TRUE(FindNotification(kScreenShareNotificationId));

  Shell::Get()->system_tray_notifier()->NotifyScreenShareStop();

  EXPECT_FALSE(FindNotification(kScreenShareNotificationId));
}

// Tests that `NotifyScreenShareStop()` does not crash if called with no
// notification with VideoConference enabled and disabled.
TEST_P(ScreenSecurityControllerTest, NotifyScreenShareStopNoNotification) {
  Shell::Get()->system_tray_notifier()->NotifyScreenShareStop();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kVideoConference);
  Shell::Get()->system_tray_notifier()->NotifyScreenShareStop();
}

// Tests that screen share notifications do not show when VideoConference is
// enabled.
TEST_P(ScreenSecurityControllerTest,
       NoScreenShareNotificationWithVideoConference) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kVideoConference);

  Shell::Get()->system_tray_notifier()->NotifyScreenShareStart(
      base::DoNothing(), std::u16string());

  EXPECT_FALSE(FindNotification(kScreenShareNotificationId));
}

// Tests that calling `NotifyScreenCaptureStop()` does not crash if called with
// no notification with VideoConference enabled and disabled.
TEST_P(ScreenSecurityControllerTest, NotifyScreenCaptureStopNoNotification) {
  Shell::Get()->system_tray_notifier()->NotifyScreenCaptureStop();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kVideoConference);
  Shell::Get()->system_tray_notifier()->NotifyScreenCaptureStop();
}

// Tests that screen capture notifications show with video conference enabled.
TEST_P(ScreenSecurityControllerTest,
       ScreenCaptureShowsNotificationWithVideoConference) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kVideoConference);

  Shell::Get()->system_tray_notifier()->NotifyScreenCaptureStart(
      base::DoNothing(), base::RepeatingClosure(), std::u16string());

  EXPECT_TRUE(FindNotification(kScreenCaptureNotificationId));

  Shell::Get()->system_tray_notifier()->NotifyScreenCaptureStop();

  EXPECT_FALSE(FindNotification(kScreenCaptureNotificationId));
}

TEST_P(ScreenSecurityControllerTest,
       DoNotShowScreenCaptureNotificationWhenCasting) {
  Shell::Get()->OnCastingSessionStartedOrStopped(true /* started */);
  Shell::Get()->system_tray_notifier()->NotifyScreenCaptureStart(
      base::DoNothing(), base::RepeatingClosure(), std::u16string());
  EXPECT_FALSE(FindNotification(kScreenCaptureNotificationId));

  Shell::Get()->system_tray_notifier()->NotifyScreenCaptureStop();
  Shell::Get()->OnCastingSessionStartedOrStopped(false /* started */);
  EXPECT_FALSE(FindNotification(kScreenCaptureNotificationId));
}

class PrivacyIndicatorsScreenSecurityTest : public AshTestBase {
 public:
  PrivacyIndicatorsScreenSecurityTest() = default;
  PrivacyIndicatorsScreenSecurityTest(
      const PrivacyIndicatorsScreenSecurityTest&) = delete;
  PrivacyIndicatorsScreenSecurityTest& operator=(
      const PrivacyIndicatorsScreenSecurityTest&) = delete;
  ~PrivacyIndicatorsScreenSecurityTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kPrivacyIndicators);

    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the screen share notification is created with proper metadata when
// the `SystemTrayNotifier` notifies observers of screen share start.
TEST_F(PrivacyIndicatorsScreenSecurityTest, ScreenShareNotification) {
  Shell::Get()->system_tray_notifier()->NotifyScreenShareStart(
      base::DoNothing(), std::u16string());

  auto* notification = FindNotification(kScreenShareNotificationId);
  EXPECT_TRUE(notification);

  // Notification should have the correct notifier id so that it will be grouped
  // with other privacy indicators notification.
  EXPECT_EQ(kPrivacyIndicatorsNotifierId, notification->notifier_id().id);

  EXPECT_EQ(ui::kColorAshPrivacyIndicatorsBackground,
            notification->accent_color_id());
}

// Tests that the privacy indicator shows up on multiple displays, if they
// displays exist before screen share starts.
TEST_F(PrivacyIndicatorsScreenSecurityTest, TrayItemIndicator) {
  // Make sure the indicator shows up on multiple displays.
  UpdateDisplay("400x300,400x300,400x300,400x300");

  ExpectPrivacyIndicatorsVisible(/*visible=*/false);

  Shell::Get()->system_tray_notifier()->NotifyScreenShareStart(
      base::DoNothing(), std::u16string());
  ExpectPrivacyIndicatorsVisible(/*visible=*/true);

  Shell::Get()->system_tray_notifier()->NotifyScreenShareStop();
  ExpectPrivacyIndicatorsVisible(/*visible=*/false);
}

}  // namespace ash
