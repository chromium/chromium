// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_tray.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/manager/display_manager.h"

namespace ash {

constexpr char kNotificationCenterTrayNoNotificationsToastId[] =
    "notification_center_tray_toast_ids.no_notifications";

class NotificationCenterTrayTestBase : public AshTestBase {
 public:
  NotificationCenterTrayTestBase(bool enable_notification_center_controller)
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        enable_notification_center_controller_(
            enable_notification_center_controller) {
    scoped_feature_list_.InitWithFeatureState(
        features::kNotificationCenterController,
        IsNotificationCenterControllerEnabled());
  }

  void SetUp() override {
    AshTestBase::SetUp();
    test_api_ = std::make_unique<NotificationCenterTestApi>();
  }

  NotificationCenterTestApi* test_api() { return test_api_.get(); }

  bool IsNotificationCenterControllerEnabled() const {
    return enable_notification_center_controller_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<NotificationCenterTestApi> test_api_;
  bool enable_notification_center_controller_ = false;
};

class NotificationCenterTrayTest
    : public NotificationCenterTrayTestBase,
      public testing::WithParamInterface<
          /*enable_notification_center_controller=*/bool> {
 public:
  NotificationCenterTrayTest()
      : NotificationCenterTrayTestBase(
            /*enable_notification_center_controller=*/GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(
    All,
    NotificationCenterTrayTest,
    /*enable_notification_center_controller=*/testing::Bool());

// Test the initial state.
TEST_P(NotificationCenterTrayTest, ShowTrayButtonOnNotificationAvailability) {
  EXPECT_FALSE(test_api()->GetTray()->GetVisible());

  std::string id = test_api()->AddNotification();
  EXPECT_TRUE(test_api()->GetTray()->GetVisible());

  message_center::MessageCenter::Get()->RemoveNotification(id, true);

  EXPECT_FALSE(test_api()->GetTray()->GetVisible());
}

// Bubble creation and destruction.
TEST_P(NotificationCenterTrayTest, ShowAndHideBubbleOnUserInteraction) {
  test_api()->AddNotification();

  auto* tray = test_api()->GetTray();

  // Clicking on the tray button should show  the bubble.
  LeftClickOn(tray);
  EXPECT_TRUE(test_api()->IsBubbleShown());

  // Clicking a second time should destroy the bubble.
  LeftClickOn(test_api()->GetTray());
  EXPECT_FALSE(test_api()->IsBubbleShown());
}

// Hitting escape after opening the bubble should destroy the bubble
// gracefully.
TEST_P(NotificationCenterTrayTest, EscapeClosesBubble) {
  auto* tray = test_api()->GetTray();
  LeftClickOn(tray);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_api()->IsBubbleShown());
}

// Removing all notifications by hitting the `clear_all_button_` should result
// in the bubble being destroyed and the tray bubble going invisible.
TEST_P(NotificationCenterTrayTest,
       ClearAllNotificationsDestroysBubbleAndHidesTray) {
  test_api()->AddNotification();
  test_api()->AddNotification();
  test_api()->AddNotification();

  auto* tray = test_api()->GetTray();

  LeftClickOn(tray);
  LeftClickOn(test_api()->GetClearAllButton());
  EXPECT_FALSE(test_api()->IsBubbleShown());
  EXPECT_FALSE(test_api()->IsTrayShown());
}

// The last notification being removed directly by the
// `message_center::MessageCenter` API should result in the bubble being
// destroyed and tray visibility being updated.
TEST_P(NotificationCenterTrayTest, NotificationsRemovedByMessageCenterApi) {
  std::string id = test_api()->AddNotification();
  test_api()->RemoveNotification(id);

  EXPECT_FALSE(test_api()->IsBubbleShown());
  EXPECT_FALSE(test_api()->IsTrayShown());
}

// When the only notifications present are all in the same group, removing the
// parent notification of that group should result in the bubble being destroyed
// and the tray being hidden.
TEST_P(NotificationCenterTrayTest,
       RemovingGroupParentDestroysBubbleAndHidesTray) {
  // Add two notifications that belong to the same group.
  const std::string url = "http://test-url.com";
  const std::string id0 = test_api()->AddNotificationWithSourceUrl(url);
  const std::string id1 = test_api()->AddNotificationWithSourceUrl(url);
  ASSERT_EQ(message_center_utils::GetNotificationCount(), 1u);
  ASSERT_TRUE(test_api()->IsTrayShown());

  // Show the bubble.
  test_api()->ToggleBubble();
  ASSERT_TRUE(test_api()->IsBubbleShown());

  // Remove the parent notification.
  const std::string parent_id =
      test_api()->NotificationIdToParentNotificationId(id0);
  test_api()->RemoveNotification(parent_id);

  // Verify that the bubble and tray are both gone.
  EXPECT_FALSE(test_api()->IsBubbleShown());
  EXPECT_FALSE(test_api()->IsTrayShown());
}

// Tests that clicking on the tray immediately after all notifications have been
// removed does not result in an empty bubble being shown. This addresses
// b/293174118.
TEST_P(NotificationCenterTrayTest, ClickOnTrayAfterRemovingNotifications) {
  // Add a notification to make the tray visible. Note that animations complete
  // immediately in this part of the test.
  std::string id = test_api()->AddNotification();
  auto* tray = test_api()->GetTray();

  // Make sure animations don't complete immediately for the rest of the test,
  // so that we can click on the tray while its running its hide animation.
  ui::ScopedAnimationDurationScaleMode animation_duration_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Remove all notifications and verify that the tray's hide animation is
  // running.
  test_api()->RemoveNotification(id);
  ASSERT_EQ(0u, test_api()->GetNotificationCount());
  EXPECT_TRUE(test_api()->IsTrayAnimating());
  EXPECT_FALSE(test_api()->GetTray()->layer()->GetTargetVisibility());
  EXPECT_EQ(test_api()->GetTray()->layer()->GetTargetOpacity(), 0);

  // Click on the tray and verify that notification bubble does not show (we do
  // not need to wait for animations here because if the bubble were to be shown
  // it would happen immediately, without animation, after clicking the tray).
  LeftClickOn(tray);
  EXPECT_FALSE(test_api()->IsBubbleShown());
}

// Tests that opening the bubble results in existing popups being dismissed
// and no new ones being created.
TEST_P(NotificationCenterTrayTest, NotificationPopupsHiddenWithBubble) {
  // Adding a notification should generate a popup.
  std::string id = test_api()->AddNotification();
  EXPECT_TRUE(test_api()->IsPopupShown(id));

  // Opening the notification center should result in the popup being dismissed.
  test_api()->ToggleBubble();
  EXPECT_FALSE(test_api()->IsPopupShown(id));

  // New notifications should not generate popups while the notification center
  // is visible.
  std::string id2 = test_api()->AddNotification();
  EXPECT_FALSE(test_api()->IsPopupShown(id));
}

// Tests that popups are shown after the notification center is closed.
TEST_P(NotificationCenterTrayTest, NotificationPopupsShownAfterBubbleClose) {
  test_api()->AddNotification();

  // Open and close bubble to dismiss existing popups.
  test_api()->ToggleBubble();
  test_api()->ToggleBubble();

  // New notifications should show up as popups after the bubble is closed.
  std::string id = test_api()->AddNotification();
  EXPECT_TRUE(test_api()->IsPopupShown(id));
}

// Keyboard accelerator shows/hides the bubble.
TEST_P(NotificationCenterTrayTest, AcceleratorTogglesBubble) {
  test_api()->AddNotification();
  EXPECT_FALSE(test_api()->IsBubbleShown());
  // Pressing the accelerator should show the bubble.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN));
  EXPECT_TRUE(test_api()->IsBubbleShown());
  // Pressing the acccelerator again should hide the bubble.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN));
  EXPECT_FALSE(test_api()->IsBubbleShown());
}

// Keyboard accelerator shows a toast when there are no notifications.
TEST_P(NotificationCenterTrayTest, AcceleratorShowsToastWhenNoNotifications) {
  ASSERT_EQ(test_api()->GetNotificationCount(), 0u);
  EXPECT_FALSE(ToastManager::Get()->IsToastShown(
      kNotificationCenterTrayNoNotificationsToastId));
  // Pressing the accelerator should show the toast and not the bubble.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN));
  EXPECT_TRUE(ToastManager::Get()->IsToastShown(
      kNotificationCenterTrayNoNotificationsToastId));
  EXPECT_FALSE(test_api()->IsBubbleShown());
}

// Tests that the bubble automatically hides if it is visible when another
// bubble becomes visible, and otherwise does not automatically show or hide.
TEST_P(NotificationCenterTrayTest, BubbleHideBehavior) {
  // Basic verification test that the notification center tray bubble can
  // show/hide itself when no other bubbles are visible.
  EXPECT_FALSE(test_api()->IsBubbleShown());
  test_api()->AddNotification();
  test_api()->ToggleBubble();
  EXPECT_TRUE(test_api()->IsBubbleShown());
  test_api()->ToggleBubble();
  EXPECT_FALSE(test_api()->IsBubbleShown());

  // Test that the notification center tray bubble automatically hides when it
  // is currently visible while another bubble becomes visible.
  test_api()->ToggleBubble();
  EXPECT_TRUE(test_api()->IsBubbleShown());
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_FALSE(test_api()->IsBubbleShown());

  // Hide all currently visible bubbles.
  GetPrimaryUnifiedSystemTray()->CloseBubble();
  EXPECT_FALSE(test_api()->IsBubbleShown());

  // Test that the notification center tray bubble stays hidden when showing
  // another bubble.
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_FALSE(test_api()->IsBubbleShown());
}

// Tests that visibility of the Do not disturb icon changes with Do not disturb
// mode.
TEST_P(NotificationCenterTrayTest, DoNotDisturbIconVisibility) {
  // Test the case where the tray is not initially visible.
  ASSERT_FALSE(test_api()->IsTrayShown());
  EXPECT_FALSE(test_api()->IsDoNotDisturbIconShown());
  message_center::MessageCenter::Get()->SetQuietMode(true);
  EXPECT_TRUE(test_api()->IsTrayShown());
  EXPECT_TRUE(test_api()->IsDoNotDisturbIconShown());
  message_center::MessageCenter::Get()->SetQuietMode(false);
  EXPECT_FALSE(test_api()->IsTrayShown());
  EXPECT_FALSE(test_api()->IsDoNotDisturbIconShown());

  // Test the case where the tray is initially visible.
  test_api()->AddNotification();
  ASSERT_TRUE(test_api()->IsTrayShown());
  EXPECT_FALSE(test_api()->IsDoNotDisturbIconShown());
  message_center::MessageCenter::Get()->SetQuietMode(true);
  EXPECT_TRUE(test_api()->IsTrayShown());
  EXPECT_TRUE(test_api()->IsDoNotDisturbIconShown());
  message_center::MessageCenter::Get()->SetQuietMode(false);
  EXPECT_TRUE(test_api()->IsTrayShown());
  EXPECT_FALSE(test_api()->IsDoNotDisturbIconShown());
}

TEST_P(NotificationCenterTrayTest, DoNotDisturbUpdatesPinnedIcons) {
  test_api()->AddPinnedNotification();
  EXPECT_TRUE(test_api()->IsNotificationIconShown());

  message_center::MessageCenter::Get()->SetQuietMode(true);
  EXPECT_FALSE(test_api()->IsNotificationIconShown());

  message_center::MessageCenter::Get()->SetQuietMode(false);
  EXPECT_TRUE(test_api()->IsNotificationIconShown());
}

TEST_P(NotificationCenterTrayTest, NoPrivacyIndicatorsWhenVcEnabled) {
  // No privacy indicators when `kVideoConference` is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kFeatureManagementVideoConference);

  auto notification_tray =
      std::make_unique<NotificationCenterTray>(GetPrimaryShelf());
  EXPECT_FALSE(notification_tray->privacy_indicators_view());
}

// Tests that the focus ring is visible and has proper size when the
// notification center tray is focused.
TEST_P(NotificationCenterTrayTest, FocusRing) {
  // Add a notification to make the notification center tray visible.
  test_api()->AddNotification();
  ASSERT_TRUE(test_api()->IsTrayShown());

  // Verify that the focus ring is not already visible.
  EXPECT_FALSE(test_api()->GetFocusRing()->GetVisible());

  // Focus the notification center tray.
  test_api()->FocusTray();

  // Verify that the focus ring is visible and is larger than the notification
  // center tray by `kTrayBackgroundFocusPadding`.
  EXPECT_TRUE(test_api()->GetFocusRing()->GetVisible());
  EXPECT_EQ(test_api()->GetFocusRing()->size(),
            test_api()->GetTray()->size() + kTrayBackgroundFocusPadding.size());
}

// Tests that removing all notifications while the lock screen is showing hides
// the tray.
TEST_P(NotificationCenterTrayTest,
       RemovingAllNotificationsOnLockScreenHidesTray) {
  // Add a notification to make the notification center tray visible.
  const std::string id = test_api()->AddNotification();

  // Show the lock screen.
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  ASSERT_TRUE(test_api()->IsTrayShown());

  // Remove the notification.
  test_api()->RemoveNotification(id);

  // Verify that the tray is hidden.
  EXPECT_FALSE(test_api()->IsTrayShown());
}

// Tests that adding an initial notification while the lock screen is showing
// shows the tray.
TEST_P(NotificationCenterTrayTest, AddingNotificationOnLockScreenShowsTray) {
  // Show the lock screen.
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  ASSERT_FALSE(test_api()->IsTrayShown());

  // Add a notification.
  test_api()->AddNotification();

  // Verify that the tray is shown.
  EXPECT_TRUE(test_api()->IsTrayShown());
}

// Tests that `NotificationCounterView` is not still visible on secondary
// display after logging in with a pinned notification present. This covers
// b/284139989.
TEST_P(NotificationCenterTrayTest,
       NotificationCounterVisibilityForMultiDisplay) {
  // The behavior under test relies on `TrayItemView` animations being
  // scheduled, but `TrayItemView` animations are bypassed when the animation
  // duration scale mode is set to ZERO_DURATION. Hence, set the animation
  // duration scale mode to something else for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // This test relies on the lock screen actually being created (and creating
  // the lock screen requires the existence of an `AuthEventsRecorder`).
  std::unique_ptr<AuthEventsRecorder> auth_events_recorder =
      AuthEventsRecorder::CreateForTesting();
  GetSessionControllerClient()->set_show_lock_screen_views(true);

  // Create two displays.
  UpdateDisplay("800x799,800x799");
  auto secondary_display_id = display_manager()->GetDisplayAt(1).id();
  auto* secondary_notification_center_tray =
      test_api()->GetTrayOnDisplay(secondary_display_id);
  auto* secondary_notification_counter_view =
      secondary_notification_center_tray->notification_icons_controller()
          ->notification_counter_view();

  // Add a pinned notification.
  test_api()->AddPinnedNotification();

  // Verify that the secondary display's notification center tray shows an icon
  // for the pinned notification and not the `NotificationCounterView`.
  ASSERT_TRUE(
      test_api()->IsNotificationIconShownOnDisplay(secondary_display_id));
  ASSERT_FALSE(secondary_notification_counter_view->GetVisible());

  // Go to the lock screen.
  GetSessionControllerClient()->LockScreen();

  // Log back in.
  GetSessionControllerClient()->UnlockScreen();

  // Verify that the `NotificationCounterView` on the secondary display is not
  // visible.
  EXPECT_FALSE(secondary_notification_counter_view->GetVisible());
}

// Tests that the privacy indicators view is created and show/hide accordingly
// when updated.
TEST_P(NotificationCenterTrayTest, PrivacyIndicatorsVisibility) {
  auto* notification_tray = StatusAreaWidgetTestHelper::GetStatusAreaWidget()
                                ->notification_center_tray();
  auto* privacy_indicators_view = notification_tray->privacy_indicators_view();
  EXPECT_TRUE(privacy_indicators_view);

  EXPECT_FALSE(privacy_indicators_view->GetVisible());

  scoped_refptr<PrivacyIndicatorsNotificationDelegate> delegate =
      base::MakeRefCounted<PrivacyIndicatorsNotificationDelegate>();

  // Updates the controller to simulate camera access, the privacy indicators
  // should become visible.
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      /*app_id=*/"app_id", /*app_name=*/u"App Name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false, delegate, PrivacyIndicatorsSource::kApps);
  EXPECT_TRUE(privacy_indicators_view->GetVisible());

  // Updates the controller to simulate that camera and microphone are not
  // accessed, the privacy indicators should be hidden.
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      /*app_id=*/"app_id", /*app_name=*/u"App Name",
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false, delegate, PrivacyIndicatorsSource::kApps);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  EXPECT_FALSE(privacy_indicators_view->GetVisible());
}

// Test fixture that disables notification popups.
class NotificationCenterTrayNoPopupsTest
    : public NotificationCenterTrayTestBase,
      public testing::WithParamInterface<
          /*enable_notification_center_controller=*/bool> {
 public:
  NotificationCenterTrayNoPopupsTest()
      : NotificationCenterTrayTestBase(
            /*enable_notification_center_controller=*/GetParam()) {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSuppressMessageCenterPopups);
    NotificationCenterTrayTestBase::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    NotificationCenterTrayNoPopupsTest,
    /*enable_notification_center_controller=*/testing::Bool());

// Tests that `NotificationCenterTray`'s `TrayItemView`s show up when adding a
// secondary display. Notification popups are disabled for this test because the
// presence of a popup actually hides the issue (i.e. the secondary display's
// `NotificationCenterTray`'s `TrayItemView`s work as intended when a popup is
// present). This covers b/281158734.
TEST_P(NotificationCenterTrayNoPopupsTest,
       TrayItemsVisibleWhenAddingSecondaryDisplay) {
  // Start with one display.
  UpdateDisplay("800x799");

  // Add a pinned notification and a non-pinned notification.
  test_api()->AddNotification();
  test_api()->AddPinnedNotification();

  // Verify that both the notification counter as well as an icon for the pinned
  // notification are visible in the notification center tray.
  ASSERT_TRUE(test_api()->IsNotificationIconShown());
  ASSERT_TRUE(test_api()->IsNotificationCounterShown());

  // Add a secondary display.
  UpdateDisplay("800x799,800x799");
  auto secondary_display_id = display_manager()->GetDisplayAt(1).id();

  // Verify that both the notification counter as well as an icon for the pinned
  // notification are visible in the secondary display's notification center
  // tray.
  EXPECT_TRUE(
      test_api()->IsNotificationIconShownOnDisplay(secondary_display_id));
  EXPECT_TRUE(
      test_api()->IsNotificationCounterShownOnDisplay(secondary_display_id));
}

// TODO(b/252875025):
// Add following test cases as we add relevant functionality:
// - Focus Change dismissing bubble
// - Popup notifications are dismissed when the bubble appears.
// - Display removed while the bubble is shown.
// - Tablet mode transition with the bubble open.

}  // namespace ash
