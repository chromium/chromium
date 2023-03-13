// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_animation_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ash/system/unified/notification_icons_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"

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
  TrayItemView* tray_item_ = nullptr;

  base::RunLoop run_loop_;

  base::WeakPtrFactory<TrayItemViewAnimationWaiter> weak_ptr_factory_{this};
};

class StatusAreaAnimationControllerTest : public AshTestBase {
 public:
  StatusAreaAnimationControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list.InitAndEnableFeature(features::kQsRevamp);
  }
  StatusAreaAnimationControllerTest(const StatusAreaAnimationControllerTest&) =
      delete;
  StatusAreaAnimationControllerTest& operator=(
      const StatusAreaAnimationControllerTest&) = delete;
  ~StatusAreaAnimationControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    test_api = std::make_unique<NotificationCenterTestApi>(
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()
            ->notification_center_tray());
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

  base::test::ScopedFeatureList scoped_feature_list;
  std::unique_ptr<NotificationCenterTestApi> test_api;
};

// Tests that the notification center tray's `TrayItemView`s' animations are
// disabled while the notification center tray is running its initial show
// animation.
TEST_F(StatusAreaAnimationControllerTest,
       TrayItemAnimationDisabledDuringShowAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ASSERT_FALSE(test_api->GetTray()->layer()->GetAnimator()->is_animating());
  ASSERT_FALSE(test_api->IsTrayShown());
  ASSERT_FALSE(notification_counter()->GetVisible());

  // Add a notification to make the notification center tray visible.
  test_api->AddNotification();
  ASSERT_TRUE(test_api->GetTray()->layer()->GetAnimator()->is_animating());
  ASSERT_TRUE(test_api->IsTrayShown());

  // Verify that the notification counter does not start animating.
  EXPECT_FALSE(IsNotificationCounterAnimationRunning());

  // Wait for the tray's show animation to finish.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->GetTray()->layer()->GetAnimator()->is_animating());

  // Verify that the notification counter is visible.
  EXPECT_TRUE(notification_counter()->GetVisible());
  EXPECT_EQ(notification_counter()->layer()->opacity(), 1);
}

// Tests that the notification center tray's `TrayItemView`s' animations are
// disabled while the notification center tray is running its show animation,
// even when it's not the initial show animation.
TEST_F(StatusAreaAnimationControllerTest,
       TrayItemAnimationDisabledDuringNonInitialShowAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ASSERT_FALSE(test_api->GetTray()->layer()->GetAnimator()->is_animating());
  ASSERT_FALSE(test_api->IsTrayShown());
  ASSERT_FALSE(notification_counter()->GetVisible());

  // Add a notification to make the notification center tray visible.
  auto id = test_api->AddNotification();
  ASSERT_TRUE(test_api->GetTray()->layer()->GetAnimator()->is_animating());

  // Wait for the tray's show animation to finish.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->GetTray()->layer()->GetAnimator()->is_animating());

  // Remove the notification, causing the tray to hide.
  test_api->RemoveNotification(id);
  ASSERT_TRUE(test_api->GetTray()->layer()->GetAnimator()->is_animating());

  // Wait for the hide animation to finish.
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->GetTray()->layer()->GetAnimator()->is_animating());
  ASSERT_FALSE(test_api->IsTrayShown());
  ASSERT_FALSE(notification_counter()->GetVisible());

  // Add another notification to make the tray visible a second time.
  test_api->AddNotification();
  ASSERT_TRUE(test_api->GetTray()->layer()->GetAnimator()->is_animating());
  ASSERT_TRUE(test_api->IsTrayShown());

  // Verify that the notification counter does not start animating.
  EXPECT_FALSE(IsNotificationCounterAnimationRunning());

  // Wait for the tray's show animation to finish.
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->GetTray()->layer()->GetAnimator()->is_animating());

  // Verify that the notification counter is visible.
  EXPECT_TRUE(notification_counter()->GetVisible());
  EXPECT_EQ(notification_counter()->layer()->opacity(), 1);
}

// Tests that already-visible `TrayItemView`s do not animate when a new
// `TrayItemView` becomes visible.
TEST_F(StatusAreaAnimationControllerTest,
       VisibleTrayItemDoesNotAnimateDuringNewTrayItemAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ASSERT_FALSE(test_api->GetTray()->layer()->GetAnimator()->is_animating());
  ASSERT_FALSE(test_api->IsTrayShown());

  // Show the tray by adding a notification.
  test_api->AddNotification();
  ASSERT_TRUE(test_api->GetTray()->layer()->GetAnimator()->is_animating());

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
  ASSERT_FALSE(test_api->GetTray()->layer()->GetAnimator()->is_animating());
  ASSERT_FALSE(test_api->IsTrayShown());

  // Show the tray by adding a notification.
  auto id = test_api->AddNotification();
  ASSERT_TRUE(test_api->GetTray()->layer()->GetAnimator()->is_animating());

  // Wait for the tray's show animation to finish.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->GetTray()->layer()->GetAnimator()->is_animating());
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
  EXPECT_FALSE(test_api->GetTray()->layer()->GetAnimator()->is_animating());

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
  ASSERT_FALSE(test_api->GetTray()->layer()->GetAnimator()->is_animating());

  // Set the animation duration scale to some small non-zero value for the rest
  // of the test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Remove the initial notification, causing the notification counter to hide.
  test_api->RemoveNotification(id);

  // Verify that the tray's hide animation is running.
  EXPECT_TRUE(test_api->GetTray()->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(test_api->GetTray()->layer()->GetTargetVisibility());
  EXPECT_EQ(test_api->GetTray()->layer()->GetTargetOpacity(), 0);

  // Add another notification to show the tray again.
  test_api->AddNotification();

  // Verify that the tray is still animating, but this time it is the show
  // animation that is running.
  EXPECT_TRUE(test_api->GetTray()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_api->GetTray()->layer()->GetTargetVisibility());
  EXPECT_EQ(test_api->GetTray()->layer()->GetTargetOpacity(), 1);

  // Wait for the show animation to finish.
  ui::LayerAnimationStoppedWaiter notification_center_tray_waiter;
  notification_center_tray_waiter.Wait(test_api->GetTray()->layer());
  ASSERT_FALSE(test_api->GetTray()->layer()->GetAnimator()->is_animating());

  // Verify that the tray is visible.
  EXPECT_TRUE(test_api->GetTray()->GetVisible());
  EXPECT_EQ(test_api->GetTray()->layer()->opacity(), 1);
}

}  // namespace ash
