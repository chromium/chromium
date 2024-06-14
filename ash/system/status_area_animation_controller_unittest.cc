// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_animation_controller.h"

#include "ash/ime/ime_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ash/system/unified/notification_icons_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/display/manager/display_manager.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center.h"

namespace ash {

class TrayItemViewAnimationWaiter {
 public:
  explicit TrayItemViewAnimationWaiter(TrayItemView* tray_item)
      : tray_item_(tray_item) {}
  TrayItemViewAnimationWaiter(const TrayItemViewAnimationWaiter&) = delete;
  TrayItemViewAnimationWaiter& operator=(const TrayItemViewAnimationWaiter&) =
      delete;
  ~TrayItemViewAnimationWaiter() = default;

  // Waits for `tray_item_`'s visibility animation to finish, or no-op if it is
  // not currently animating.
  void Wait() {
    if (tray_item_->IsAnimating()) {
      tray_item_->SetAnimationIdleClosureForTest(base::BindOnce(
          &TrayItemViewAnimationWaiter::OnTrayItemAnimationFinished,
          weak_ptr_factory_.GetWeakPtr()));
      run_loop_.Run();
    }
  }

 private:
  // Called when `tray_item_`'s visibility animation finishes.
  void OnTrayItemAnimationFinished() { run_loop_.Quit(); }

  // The tray item whose animation is being waited for.
  raw_ptr<TrayItemView> tray_item_ = nullptr;

  base::RunLoop run_loop_;

  base::WeakPtrFactory<TrayItemViewAnimationWaiter> weak_ptr_factory_{this};
};

class StatusAreaAnimationControllerTest : public AshTestBase {
 public:
  StatusAreaAnimationControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  }
  StatusAreaAnimationControllerTest(const StatusAreaAnimationControllerTest&) =
      delete;
  StatusAreaAnimationControllerTest& operator=(
      const StatusAreaAnimationControllerTest&) = delete;
  ~StatusAreaAnimationControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    test_api = std::make_unique<NotificationCenterTestApi>();
    // Tray visibility animations may still be disabled due to changes in
    // session state not fully propagating. Running all pending tasks guarantees
    // that the necessary scoped closure runners are executed, thus ensuring
    // that visibility animations are enabled before proceeding with the rest of
    // the test.
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(test_api->GetTray()->IsShowAnimationEnabled());
  }

  bool IsNotificationCounterAnimationRunning() {
    return notification_counter()->IsAnimating();
  }

  bool IsCapsLockTrayItemAnimationRunning() {
    return caps_lock_tray_item()->IsAnimating();
  }

  NotificationCounterView* notification_counter() {
    return test_api->GetTray()
        ->notification_icons_controller()
        ->notification_counter_view();
  }

  NotificationIconTrayItemView* caps_lock_tray_item() {
    // The last tray item will be used to show the caps lock notification icon.
    return test_api->GetTray()
        ->notification_icons_controller()
        ->tray_items()
        .back();
  }

  std::unique_ptr<NotificationCenterTestApi> test_api;
};

// Tests that the notification center tray's `TrayItemView`s' animations are
// disabled while the notification center tray is running its initial show
// animation.
TEST_F(StatusAreaAnimationControllerTest,
       TrayItemAnimationDisabledDuringShowAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ASSERT_FALSE(test_api->IsTrayAnimating());
  ASSERT_FALSE(test_api->IsTrayShown());
  ASSERT_FALSE(notification_counter()->GetVisible());

  // Add a notification to make the notification center tray visible.
  test_api->AddNotification();
  ASSERT_TRUE(test_api->IsTrayAnimating());
  ASSERT_TRUE(test_api->IsTrayShown());

  // Verify that the notification counter does not start animating.
  EXPECT_FALSE(IsNotificationCounterAnimationRunning());

  // Wait for the tray's show animation to finish.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());

  // Verify that the notification counter is visible.
  EXPECT_TRUE(notification_counter()->GetVisible());
  EXPECT_EQ(notification_counter()->layer()->opacity(), 1);
}

// Tests that the notification center tray's `TrayItemView`s' animations are
// disabled while the notification center tray is running its initial show
// animation on a secondary display.
TEST_F(StatusAreaAnimationControllerTest,
       TrayItemAnimationDisabledDuringShowAnimationOnSecondaryDisplay) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create two displays.
  UpdateDisplay("800x799,800x799");
  auto secondary_display_id = display_manager()->GetDisplayAt(1).id();

  // Add a notification to make the notification center tray visible on both
  // displays.
  test_api->AddNotification();
  ASSERT_TRUE(test_api->IsTrayShown());
  ASSERT_TRUE(test_api->IsTrayShownOnDisplay(secondary_display_id));
  ASSERT_TRUE(test_api->IsTrayAnimating());
  ASSERT_TRUE(test_api->IsTrayAnimatingOnDisplay(secondary_display_id));

  // Verify that the notification counter on the secondary display does not
  // start animating.
  EXPECT_FALSE(
      test_api->IsNotificationCounterAnimatingOnDisplay(secondary_display_id));

  // Wait for the secondary display's tray show animation to finish.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(
      test_api->GetTrayOnDisplay(secondary_display_id)->layer());
  ASSERT_FALSE(test_api->IsTrayAnimatingOnDisplay(secondary_display_id));

  // Verify that the secondary display's notification counter is visible.
  EXPECT_TRUE(
      test_api->IsNotificationCounterShownOnDisplay(secondary_display_id));
  EXPECT_EQ(test_api->GetNotificationCounterOnDisplay(secondary_display_id)
                ->layer()
                ->opacity(),
            1);
}

// Tests that the notification center tray's `TrayItemView`s' animations are
// disabled while the notification center tray is running its initial hide
// animation.
TEST_F(StatusAreaAnimationControllerTest,
       TrayItemAnimationDisabledDuringHideAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ASSERT_FALSE(test_api->IsTrayAnimating());
  ASSERT_FALSE(test_api->IsTrayShown());
  ASSERT_FALSE(notification_counter()->GetVisible());

  // Add a notification to make the notification center tray visible.
  auto id = test_api->AddNotification();
  ASSERT_TRUE(test_api->IsTrayAnimating());
  ASSERT_TRUE(test_api->IsTrayShown());

  // Wait for the notification center tray's show animation to finish.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());

  // Verify that the notification center tray is visible.
  ASSERT_TRUE(test_api->IsTrayShown());

  // Remove the notification to cause the notification center tray to start its
  // hide animation.
  test_api->RemoveNotification(id);

  // Verify that the notification center tray is running its hide animation.
  ASSERT_TRUE(test_api->IsTrayAnimating());
  ASSERT_FALSE(test_api->GetTray()->layer()->GetTargetVisibility());
  ASSERT_EQ(test_api->GetTray()->layer()->GetTargetOpacity(), 0);

  // Verify that the notification counter does not start animating.
  EXPECT_FALSE(IsNotificationCounterAnimationRunning());

  // Wait for the tray's hide animation to finish.
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());

  // Verify that the notification counter is not visible.
  EXPECT_FALSE(notification_counter()->GetVisible());
  EXPECT_EQ(notification_counter()->layer()->opacity(), 0);
}

// Tests that `NotificationIconTrayItemView`s are not reset until the end of the
// `NotificationCenterTray`'s hide animation. This was added for b/284991444.
TEST_F(StatusAreaAnimationControllerTest,
       NotificationIconNotResetWhileNotificationTrayHideAnimationRunning) {
  // Note that animations still finish immediately at this stage of the test.

  // Add a critical warning system notification.
  auto id = test_api->AddCriticalWarningSystemNotification();
  auto* notification_icon = test_api->GetNotificationIconForId(id);
  CHECK(notification_icon);
  auto icon_image = notification_icon->image_view()->GetImage();

  // Verify that the notification icon is showing in the notification tray, and
  // that the tray is finished animating.
  ASSERT_TRUE(test_api->IsNotificationIconShown());
  ASSERT_TRUE(test_api->IsTrayShown());
  ASSERT_FALSE(test_api->IsTrayAnimating());

  // Set the animation duration scale to some small non-zero value for the rest
  // of the test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Remove the notification to start the notification tray's hide animation.
  test_api->RemoveNotification(id);
  ASSERT_TRUE(test_api->IsTrayAnimating());
  ASSERT_FALSE(test_api->GetTray()->layer()->GetTargetVisibility());
  ASSERT_EQ(test_api->GetTray()->layer()->GetTargetOpacity(), 0);

  // Verify that the notification icon has not been reset yet.
  EXPECT_TRUE(icon_image.BackedBySameObjectAs(
      notification_icon->image_view()->GetImage()));

  // Wait for the notification tray to finish its hide animation.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());

  // Verify that the notification icon has been reset.
  EXPECT_TRUE(notification_icon->image_view()->GetImage().BackedBySameObjectAs(
      gfx::ImageSkia()));
}

// Tests that `NotificationIconTrayItemView`s are not reset until the end of
// their hide animation. This was added for b/284991444.
TEST_F(StatusAreaAnimationControllerTest,
       NotificationIconNotResetWhileHideAnimationRunning) {
  // Note that animations still finish immediately at this stage of the test.

  // Add a standard notification as well as a critical warning system
  // notification.
  test_api->AddNotification();
  auto id = test_api->AddCriticalWarningSystemNotification();
  auto* notification_icon = test_api->GetNotificationIconForId(id);
  CHECK(notification_icon);
  auto icon_image = notification_icon->image_view()->GetImage();

  // Verify that both the notification counter and a notification icon are
  // showing in the notification tray, and that the tray is finished animating.
  ASSERT_TRUE(test_api->IsNotificationCounterShown());
  ASSERT_TRUE(test_api->IsNotificationIconShown());
  ASSERT_TRUE(test_api->IsTrayShown());
  ASSERT_FALSE(test_api->IsTrayAnimating());

  // Set the animation duration scale to some small non-zero value for the
  // rest of the test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Remove the critical warning system notification to start the notification
  // icon's hide animation (note that the notification tray should remain
  // visible due to the presence of the standard notification).
  test_api->RemoveNotification(id);
  ASSERT_FALSE(test_api->IsTrayAnimating());
  ASSERT_FALSE(test_api->IsNotificationCounterAnimating());
  ASSERT_TRUE(notification_icon->IsAnimating());
  ASSERT_FALSE(notification_icon->target_visible_for_testing());

  // Verify that the notification icon has not been reset yet.
  EXPECT_TRUE(icon_image.BackedBySameObjectAs(
      notification_icon->image_view()->GetImage()));

  // End the notification icon's hide animation.
  auto* icon_animation = notification_icon->animation_for_testing();
  icon_animation->End();
  ASSERT_FALSE(notification_icon->IsAnimating());

  // Verify that the notification icon has been reset.
  EXPECT_TRUE(notification_icon->image_view()->GetImage().BackedBySameObjectAs(
      gfx::ImageSkia()));
}

// Tests that the notification center tray's `TrayItemView`s' animations are
// disabled while the notification center tray is running its show animation,
// even when it's not the initial show animation.
TEST_F(StatusAreaAnimationControllerTest,
       TrayItemAnimationDisabledDuringNonInitialShowAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ASSERT_FALSE(test_api->IsTrayAnimating());
  ASSERT_FALSE(test_api->IsTrayShown());
  ASSERT_FALSE(notification_counter()->GetVisible());

  // Add a notification to make the notification center tray visible.
  auto id = test_api->AddNotification();
  ASSERT_TRUE(test_api->IsTrayAnimating());

  // Wait for the tray's show animation to finish.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());

  // Remove the notification, causing the tray to hide.
  test_api->RemoveNotification(id);
  ASSERT_TRUE(test_api->IsTrayAnimating());

  // Wait for the hide animation to finish.
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());
  ASSERT_FALSE(test_api->IsTrayShown());
  ASSERT_FALSE(notification_counter()->GetVisible());

  // Add another notification to make the tray visible a second time.
  test_api->AddNotification();
  ASSERT_TRUE(test_api->IsTrayAnimating());
  ASSERT_TRUE(test_api->IsTrayShown());

  // Verify that the notification counter does not start animating.
  EXPECT_FALSE(IsNotificationCounterAnimationRunning());

  // Wait for the tray's show animation to finish.
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());

  // Verify that the notification counter is visible.
  EXPECT_TRUE(notification_counter()->GetVisible());
  EXPECT_EQ(notification_counter()->layer()->opacity(), 1);
}

// Tests that the notification center tray's `TrayItemView`s' animations are
// disabled while the notification center tray is running its hide animation,
// even when it's not the initial hide animation.
TEST_F(StatusAreaAnimationControllerTest,
       TrayItemAnimationDisabledDuringNonInitialHideAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ASSERT_FALSE(test_api->IsTrayAnimating());
  ASSERT_FALSE(test_api->IsTrayShown());
  ASSERT_FALSE(notification_counter()->GetVisible());

  // Add a notification to make the notification center tray visible.
  auto id = test_api->AddNotification();
  ASSERT_TRUE(test_api->IsTrayAnimating());

  // Wait for the tray's show animation to finish.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());
  ASSERT_TRUE(test_api->IsTrayShown());
  ASSERT_TRUE(notification_counter()->GetVisible());

  // Remove the notification, causing the tray to hide.
  test_api->RemoveNotification(id);
  ASSERT_TRUE(test_api->IsTrayAnimating());

  // Wait for the hide animation to finish.
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());
  ASSERT_FALSE(test_api->IsTrayShown());
  ASSERT_FALSE(notification_counter()->GetVisible());

  // Add another notification to make the tray visible a second time.
  id = test_api->AddNotification();
  ASSERT_TRUE(test_api->IsTrayAnimating());

  // Wait for the notification center tray's second show animation to finish.
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());
  ASSERT_TRUE(test_api->IsTrayShown());
  ASSERT_TRUE(notification_counter()->GetVisible());

  // Remove the notification, causing the notification center tray to start its
  // second hide animation.
  test_api->RemoveNotification(id);
  ASSERT_TRUE(test_api->IsTrayAnimating());

  // Verify that the notification counter does not start animating.
  EXPECT_FALSE(IsNotificationCounterAnimationRunning());

  // Wait for the notification center tray's hide animation to finish.
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());
  ASSERT_FALSE(test_api->IsTrayShown());

  // Verify that the notification counter is hidden.
  EXPECT_FALSE(notification_counter()->GetVisible());
  EXPECT_EQ(notification_counter()->layer()->opacity(), 0);
}

// Tests that already-visible `TrayItemView`s do not animate when a new
// `TrayItemView` becomes visible.
TEST_F(StatusAreaAnimationControllerTest,
       VisibleTrayItemDoesNotAnimateDuringNewTrayItemAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ASSERT_FALSE(test_api->IsTrayAnimating());
  ASSERT_FALSE(test_api->IsTrayShown());

  // Show the tray by adding a notification.
  test_api->AddNotification();
  ASSERT_TRUE(test_api->IsTrayAnimating());

  // Wait for the tray's show animation to finish.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_TRUE(notification_counter()->GetVisible());
  ASSERT_FALSE(IsNotificationCounterAnimationRunning());

  // Add a second icon to the tray (caps lock).
  Shell::Get()->ime_controller()->UpdateCapsLockState(true);

  // Verify that the new tray item (caps lock) animates in while the existing
  // tray item (counter) does not animate.
  EXPECT_TRUE(IsCapsLockTrayItemAnimationRunning());
  EXPECT_FALSE(IsNotificationCounterAnimationRunning());

  // Wait for the caps lock tray item show animation to finish.
  TrayItemViewAnimationWaiter(caps_lock_tray_item()).Wait();
  EXPECT_FALSE(IsCapsLockTrayItemAnimationRunning());

  // Verify that both tray items are visible.
  EXPECT_TRUE(caps_lock_tray_item()->GetVisible());
  EXPECT_EQ(caps_lock_tray_item()->layer()->opacity(), 1);
  EXPECT_TRUE(notification_counter()->GetVisible());
  EXPECT_EQ(notification_counter()->layer()->opacity(), 1);
}

// Tests that visible `TrayItemView`s can animate out without causing the entire
// notification center tray to animate when there are at least two items in the
// tray.
TEST_F(StatusAreaAnimationControllerTest,
       VisibleTrayItemAnimatesOutWithoutCausingWholeTrayAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ASSERT_FALSE(test_api->IsTrayAnimating());
  ASSERT_FALSE(test_api->IsTrayShown());

  // Show the tray by adding a notification.
  auto id = test_api->AddNotification();
  ASSERT_TRUE(test_api->IsTrayAnimating());

  // Wait for the tray's show animation to finish.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());
  ASSERT_TRUE(notification_counter()->GetVisible());

  // Add a second icon (caps lock) to the tray (this also adds a second
  // notification).
  Shell::Get()->ime_controller()->UpdateCapsLockState(true);

  // Wait for the caps lock tray item show animation to finish.
  TrayItemViewAnimationWaiter(caps_lock_tray_item()).Wait();
  ASSERT_FALSE(IsCapsLockTrayItemAnimationRunning());
  ASSERT_TRUE(caps_lock_tray_item()->GetVisible());

  // Remove the initial notification, causing the notification counter to hide.
  test_api->RemoveNotification(id);
  ASSERT_EQ(test_api->GetNotificationCount(), 1u);

  // Verify that the notification counter animates out while the tray remains
  // visible.
  EXPECT_TRUE(IsNotificationCounterAnimationRunning());
  EXPECT_TRUE(test_api->GetTray()->GetVisible());
  EXPECT_FALSE(test_api->IsTrayAnimating());

  // Wait for the notification counter hide animation to finish.
  TrayItemViewAnimationWaiter(notification_counter()).Wait();
  EXPECT_FALSE(IsNotificationCounterAnimationRunning());

  // Verify that the notification counter is no longer visible, but that the
  // tray is.
  EXPECT_FALSE(notification_counter()->GetVisible());
  EXPECT_TRUE(test_api->GetTray()->GetVisible());
}

// Tests that scheduling the show animation while the hide animation is running
// results in the `TrayBackgroundView` being visible once all animations are
// finished.
TEST_F(StatusAreaAnimationControllerTest, ShowWhileHideAnimationIsRunning) {
  // Show the tray by adding a notification. Note that animations still finish
  // immediately at this stage of the test.
  auto id = test_api->AddNotification();
  ASSERT_TRUE(test_api->IsTrayShown());
  ASSERT_FALSE(test_api->IsTrayAnimating());

  // Set the animation duration scale to some small non-zero value for the rest
  // of the test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Remove the notification, causing the notification tray to hide.
  test_api->RemoveNotification(id);

  // Verify that the tray's hide animation is running.
  EXPECT_TRUE(test_api->IsTrayAnimating());
  EXPECT_FALSE(test_api->GetTray()->layer()->GetTargetVisibility());
  EXPECT_EQ(test_api->GetTray()->layer()->GetTargetOpacity(), 0);

  // Add another notification to show the tray again.
  test_api->AddNotification();

  // Verify that the tray is still animating, but this time it is the show
  // animation that is running.
  EXPECT_TRUE(test_api->IsTrayAnimating());
  EXPECT_TRUE(test_api->GetTray()->layer()->GetTargetVisibility());
  EXPECT_EQ(test_api->GetTray()->layer()->GetTargetOpacity(), 1);

  // Wait for the show animation to finish.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());

  // Verify that the tray is visible.
  EXPECT_TRUE(test_api->GetTray()->GetVisible());
  EXPECT_EQ(test_api->GetTray()->layer()->opacity(), 1);
}

// Tests that scheduling the hide animation while the show animation is running
// results in the `TrayBackgroundView` being hidden once all animations are
// finished.
TEST_F(StatusAreaAnimationControllerTest, HideWhileShowAnimationIsRunning) {
  ASSERT_FALSE(test_api->IsTrayShown());
  ASSERT_FALSE(test_api->IsTrayAnimating());

  // Set the animation duration scale to some small non-zero value for the rest
  // of the test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Cause the notification tray to start to show by adding a notification.
  auto id = test_api->AddNotification();

  // Verify that the notification tray's show animation is running.
  ASSERT_TRUE(test_api->IsTrayAnimating());
  ASSERT_TRUE(test_api->GetTray()->layer()->GetTargetVisibility());
  ASSERT_EQ(test_api->GetTray()->layer()->GetTargetOpacity(), 1);

  // Remove the notification, causing the in-progress show animation to be
  // interrupted by the hide animation.
  test_api->RemoveNotification(id);

  // Verify that the tray is still animating, but this time it is the hide
  // animation that is running.
  EXPECT_TRUE(test_api->IsTrayAnimating());
  EXPECT_FALSE(test_api->GetTray()->layer()->GetTargetVisibility());
  EXPECT_EQ(test_api->GetTray()->layer()->GetTargetOpacity(), 0);

  // Wait for the hide animation to finish.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());

  // Verify that the tray is hidden.
  EXPECT_FALSE(test_api->IsTrayShown());
  EXPECT_EQ(test_api->GetTray()->layer()->opacity(), 0);
}

// Tests that the notification center tray's tray items have their animations
// reset when the tray's hide animation ends.
TEST_F(StatusAreaAnimationControllerTest,
       HideAnimationEndedResetsTrayItemAnimation) {
  // Show the tray by adding a notification. Note that animations still finish
  // immediately at this stage of the test.
  auto id = test_api->AddNotification();
  ASSERT_TRUE(test_api->IsTrayShown());
  ASSERT_FALSE(test_api->IsTrayAnimating());

  // Set the animation duration scale to some small non-zero value for the rest
  // of the test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Toggle "Do not disturb" mode on and off to ensure that the notification
  // counter animates at least once (otherwise its animation object will still
  // be null and the rest of this test will crash).
  message_center::MessageCenter::Get()->SetQuietMode(true);
  message_center::MessageCenter::Get()->SetQuietMode(false);

  // Verify that the notification counter's animation is in its "shown" state.
  ASSERT_EQ(notification_counter()->animation_for_testing()->GetCurrentValue(),
            1);

  // Remove the notification to hide the notification center tray.
  test_api->RemoveNotification(id);
  ASSERT_TRUE(test_api->IsTrayAnimating());

  // Wait for the hide animation to finish.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->IsTrayAnimating());

  // Verify that the notification counter's animation is in its "hidden" state.
  EXPECT_EQ(notification_counter()->animation_for_testing()->GetCurrentValue(),
            0);
}

}  // namespace ash
