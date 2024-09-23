// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_event_observer.h"

#include <memory>

#include "ash/display/projecting_observer.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/power/power_event_observer_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/lock_state_controller_test_api.h"
#include "ash/wm/test/test_session_state_animator.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/manager/test/fake_display_snapshot.h"

namespace ash {

class PowerEventObserverTest : public AshTestBase {
 public:
  PowerEventObserverTest() = default;

  PowerEventObserverTest(const PowerEventObserverTest&) = delete;
  PowerEventObserverTest& operator=(const PowerEventObserverTest&) = delete;

  ~PowerEventObserverTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    observer_ = Shell::Get()->power_event_observer();
  }

  void TearDown() override { AshTestBase::TearDown(); }

 protected:
  int GetNumVisibleCompositors() {
    int result = 0;
    for (aura::Window* window : Shell::GetAllRootWindows()) {
      if (window->GetHost()->compositor()->IsVisible())
        ++result;
    }

    return result;
  }

  bool GetLockedState() {
    // LockScreen is an async mojo call.
    GetSessionControllerClient()->FlushForTest();
    return Shell::Get()->session_controller()->IsScreenLocked();
  }

  raw_ptr<PowerEventObserver, DanglingUntriaged> observer_ = nullptr;
};

TEST_F(PowerEventObserverTest, LockBeforeSuspend) {
  chromeos::FakePowerManagerClient* client =
      chromeos::FakePowerManagerClient::Get();
  ASSERT_EQ(0, client->num_pending_suspend_readiness_callbacks());

  // Check that the observer requests a suspend-readiness callback when it hears
  // that the system is about to suspend.
  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());

  // It should run the callback when it hears that the screen is locked and the
  // lock screen animations have completed.
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);

  PowerEventObserverTestApi test_api(observer_);

  ui::Compositor* compositor =
      Shell::GetPrimaryRootWindow()->GetHost()->compositor();

  test_api.CompositingDidCommit(compositor);
  observer_->OnLockAnimationsComplete();

  // Verify that CompositingStarted and CompositingAckDeprecated observed before
  // CompositingDidCommit are ignored.
  test_api.CompositingStarted(compositor);
  test_api.CompositingAckDeprecated(compositor);
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(1, GetNumVisibleCompositors());

  // Suspend should remain delayed after first compositing cycle ends.
  test_api.CompositeFrame(compositor);
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(1, GetNumVisibleCompositors());

  test_api.CompositingDidCommit(compositor);
  test_api.CompositingStarted(compositor);
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(1, GetNumVisibleCompositors());

  test_api.CompositingAckDeprecated(compositor);
  EXPECT_EQ(0, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(0, GetNumVisibleCompositors());

  // If the system is already locked, no callback should be requested.
  observer_->SuspendDoneEx(power_manager::SuspendDone());
  EXPECT_EQ(1, GetNumVisibleCompositors());
  UnblockUserSession();
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);

  // Notify that lock animation is complete.
  observer_->OnLockAnimationsComplete();

  // Wait for a compositing after lock animation completes before suspending.
  // In this case compositors should be made invisible immediately
  test_api.CompositeFrame(compositor);
  test_api.CompositeFrame(compositor);

  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(0, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(0, GetNumVisibleCompositors());

  // It also shouldn't request a callback if it isn't instructed to lock the
  // screen.
  observer_->SuspendDoneEx(power_manager::SuspendDone());
  UnblockUserSession();
  SetShouldLockScreenAutomatically(false);
  EXPECT_EQ(1, GetNumVisibleCompositors());
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(0, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(0, GetNumVisibleCompositors());
}

TEST_F(PowerEventObserverTest, SetInvisibleBeforeSuspend) {
  // Tests that all the Compositors are marked invisible before a suspend
  // request when the screen is not supposed to be locked before a suspend.
  EXPECT_EQ(1, GetNumVisibleCompositors());

  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(0, GetNumVisibleCompositors());
  observer_->SuspendDoneEx(power_manager::SuspendDone());

  // Tests that all the Compositors are marked invisible _after_ the screen lock
  // animations have completed.
  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);

  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, GetNumVisibleCompositors());

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_EQ(1, GetNumVisibleCompositors());

  observer_->OnLockAnimationsComplete();

  EXPECT_EQ(1, GetNumVisibleCompositors());
  ASSERT_TRUE(PowerEventObserverTestApi(observer_)
                  .SimulateCompositorsReadyForSuspend());
  EXPECT_EQ(0, GetNumVisibleCompositors());

  observer_->SuspendDoneEx(power_manager::SuspendDone());
  EXPECT_EQ(1, GetNumVisibleCompositors());
}

TEST_F(PowerEventObserverTest, CanceledSuspend) {
  // Tests that the Compositors are not marked invisible if a suspend is
  // canceled or the system resumes before the lock screen is ready.
  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, GetNumVisibleCompositors());

  observer_->SuspendDoneEx(power_manager::SuspendDone());
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  observer_->OnLockAnimationsComplete();
  EXPECT_EQ(1, GetNumVisibleCompositors());
}

TEST_F(PowerEventObserverTest, DelayResuspendForLockAnimations) {
  // Tests that the following order of events is handled correctly:
  //
  // - A suspend request is started.
  // - The screen is locked.
  // - The suspend request is canceled.
  // - Another suspend request is started.
  // - The screen lock animations complete.
  // - The screen lock UI changes get composited
  //
  // In this case, the observer should block the second suspend request until
  // the screen lock compositing is done.
  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);

  chromeos::FakePowerManagerClient* client =
      chromeos::FakePowerManagerClient::Get();
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  observer_->SuspendDoneEx(power_manager::SuspendDone());
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);

  // The expected number of suspend readiness callbacks is 2 because the
  // observer has not run the callback that it got from the first suspend
  // request.  The real PowerManagerClient would reset its internal counter in
  // this situation but the stub client is not that smart.
  EXPECT_EQ(2, client->num_pending_suspend_readiness_callbacks());

  observer_->OnLockAnimationsComplete();
  EXPECT_EQ(2, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(1, GetNumVisibleCompositors());

  ASSERT_TRUE(PowerEventObserverTestApi(observer_)
                  .SimulateCompositorsReadyForSuspend());
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(0, GetNumVisibleCompositors());
}

// Tests that device suspend is delayed for screen lock until the screen lock
// changes are composited for all root windows.
TEST_F(PowerEventObserverTest, DelaySuspendForCompositing_MultiDisplay) {
  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);

  UpdateDisplay("200x100,300x200");

  chromeos::FakePowerManagerClient* client =
      chromeos::FakePowerManagerClient::Get();
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());

  aura::Window::Windows windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, windows.size());

  ui::Compositor* primary_compositor = windows[0]->GetHost()->compositor();
  ui::Compositor* secondary_compositor = windows[1]->GetHost()->compositor();
  ASSERT_EQ(2, GetNumVisibleCompositors());
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());

  PowerEventObserverTestApi test_api(observer_);

  // Simulate a commit before lock animations complete, and verify associated
  // compositing ends are ignored.
  test_api.CompositingDidCommit(secondary_compositor);
  observer_->OnLockAnimationsComplete();

  test_api.CompositingStarted(secondary_compositor);
  test_api.CompositingAckDeprecated(secondary_compositor);

  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(2, GetNumVisibleCompositors());

  test_api.CompositeFrame(primary_compositor);
  test_api.CompositeFrame(primary_compositor);

  test_api.CompositeFrame(secondary_compositor);

  // Even though compositing for one display is done, changes to compositor
  // visibility, and suspend readiness state should be delayed until compositing
  // for the other display finishes.
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(2, GetNumVisibleCompositors());

  test_api.CompositeFrame(secondary_compositor);
  EXPECT_EQ(0, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(0, GetNumVisibleCompositors());
}

TEST_F(PowerEventObserverTest,
       DISABLED_DelaySuspendForCompositing_PendingDisplayRemoved) {
  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);

  UpdateDisplay("200x100,300x200");

  chromeos::FakePowerManagerClient* client =
      chromeos::FakePowerManagerClient::Get();
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());

  aura::Window::Windows windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, windows.size());

  ui::Compositor* primary_compositor = windows[0]->GetHost()->compositor();
  ASSERT_EQ(2, GetNumVisibleCompositors());
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());
  observer_->OnLockAnimationsComplete();

  PowerEventObserverTestApi test_api(observer_);

  test_api.CompositeFrame(primary_compositor);
  test_api.CompositeFrame(primary_compositor);

  // Even though compositing for one display is done, changes to compositor
  // visibility, and suspend readiness state should be delayed until compositing
  // for the other display finishes.
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(2, GetNumVisibleCompositors());

  // Remove the second display, and verify the remaining compositor is hidden
  // at this point.
  UpdateDisplay("200x100");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(0, GetNumVisibleCompositors());
}

TEST_F(PowerEventObserverTest, CompositorNotVisibleAtLockAnimationsComplete) {
  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);

  chromeos::FakePowerManagerClient* client =
      chromeos::FakePowerManagerClient::Get();
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());

  Shell::GetPrimaryRootWindow()->GetHost()->compositor()->SetVisible(false);

  observer_->OnLockAnimationsComplete();
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(0, GetNumVisibleCompositors());
}

// Tests that for suspend imminent induced locking screen, locking animations
// are immediate.
TEST_F(PowerEventObserverTest, ImmediateLockAnimations) {
  TestSessionStateAnimator* test_animator = new TestSessionStateAnimator;
  LockStateController* lock_state_controller =
      Shell::Get()->lock_state_controller();
  lock_state_controller->set_animator_for_test(test_animator);
  LockStateControllerTestApi lock_state_test_api(lock_state_controller);
  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);
  ASSERT_FALSE(GetLockedState());

  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  // Tests that locking animation starts.
  EXPECT_TRUE(lock_state_test_api.is_animating_lock());

  // Tests that we have two active animation containers for pre-lock animation,
  // which are non lock screen containers and shelf container.
  EXPECT_EQ(2u, test_animator->GetAnimationCount());
  test_animator->AreContainersAnimated(
      LockStateController::kPreLockContainersMask,
      SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY);
  // Tests that after finishing immediate animation, we have no active
  // animations left.
  test_animator->Advance(test_animator->GetDuration(
      SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE));
  EXPECT_EQ(0u, test_animator->GetAnimationCount());

  // Flushes locking screen async request to start post-lock animation.
  EXPECT_TRUE(GetLockedState());
  EXPECT_TRUE(lock_state_test_api.is_animating_lock());
  // Tests that we have two active animation container for post-lock animation,
  // which are lock screen containers and shelf container.
  EXPECT_EQ(2u, test_animator->GetAnimationCount());
  test_animator->AreContainersAnimated(
      SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN);
  test_animator->AreContainersAnimated(SessionStateAnimator::SHELF,
                                       SessionStateAnimator::ANIMATION_FADE_IN);
  // Tests that after finishing immediate animation, we have no active
  // animations left. Also checks that animation ends.
  test_animator->Advance(test_animator->GetDuration(
      SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE));
  EXPECT_EQ(0u, test_animator->GetAnimationCount());
  EXPECT_FALSE(lock_state_test_api.is_animating_lock());
}

// Tests that displays will not be considered ready to suspend until the
// animated wallpaper change finishes (if the wallpaper is being animated to
// another wallpaper after the screen is locked).
// Flaky: https://crbug.com/1293178
TEST_F(PowerEventObserverTest,
       DISABLED_DisplaysNotReadyForSuspendUntilWallpaperAnimationEnds) {
  chromeos::FakePowerManagerClient* client =
      chromeos::FakePowerManagerClient::Get();
  ASSERT_EQ(0, client->num_pending_suspend_readiness_callbacks());

  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);

  // Set up animation state so wallpaper widget animations are not ended on
  // their creation.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Lock screen - this is expected to start wallpaper change (e.g. to a
  // widget with a blurred wallpaper).
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  observer_->OnLockAnimationsComplete();

  WallpaperWidgetController* wallpaper_widget_controller =
      Shell::GetPrimaryRootWindowController()->wallpaper_widget_controller();
  // Assert that the wallpaper is being animated here - otherwise the test will
  // not work.
  ASSERT_TRUE(wallpaper_widget_controller->IsAnimating());

  ui::Compositor* compositor =
      Shell::GetPrimaryRootWindow()->GetHost()->compositor();
  PowerEventObserverTestApi test_api(observer_);

  // Simulate a single frame getting composited before the wallpaper animation
  // is done - this frame is expected to be ignored by power event observer's
  // compositing state observer.
  test_api.CompositeFrame(compositor);

  // Simulate wallpaper animation finishing - for the purpose of this test,
  // before suspend begins.
  wallpaper_widget_controller->StopAnimating();

  // Expect that two compositing cycles are completed before suspend continues,
  // and displays get suspended.
  test_api.CompositeFrame(compositor);
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(1, GetNumVisibleCompositors());

  test_api.CompositeFrame(compositor);
  EXPECT_EQ(0, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(0, GetNumVisibleCompositors());
}

// Tests that animated wallpaper changes will be finished immediately when
// suspend starts (if the screen was locked when suspend started).
TEST_F(PowerEventObserverTest, EndWallpaperAnimationOnSuspendWhileLocked) {
  chromeos::FakePowerManagerClient* client =
      chromeos::FakePowerManagerClient::Get();
  ASSERT_EQ(0, client->num_pending_suspend_readiness_callbacks());

  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);

  // Set up animation state so wallpaper widget animations are not ended on
  // their creation.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Lock screen - this is expected to start wallpaper change (e.g. to a
  // widget with a blurred wallpaper).
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  observer_->OnLockAnimationsComplete();

  // Wallpaper animation should be stopped immediately on suspend.
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);

  WallpaperWidgetController* wallpaper_widget_controller =
      Shell::GetPrimaryRootWindowController()->wallpaper_widget_controller();
  EXPECT_FALSE(wallpaper_widget_controller->IsAnimating());

  ui::Compositor* compositor =
      Shell::GetPrimaryRootWindow()->GetHost()->compositor();
  PowerEventObserverTestApi test_api(observer_);

  // Expect that two compositing cycles are completed before suspend continues,
  // and displays get suspended.
  test_api.CompositeFrame(compositor);
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(1, GetNumVisibleCompositors());

  test_api.CompositeFrame(compositor);
  EXPECT_EQ(0, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(0, GetNumVisibleCompositors());
}

// Tests that animated wallpaper changes will be finished immediately when
// suspend starts (if the screen lock started before suspend).
TEST_F(PowerEventObserverTest, EndWallpaperAnimationOnSuspendWhileLocking) {
  chromeos::FakePowerManagerClient* client =
      chromeos::FakePowerManagerClient::Get();
  ASSERT_EQ(0, client->num_pending_suspend_readiness_callbacks());

  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);

  // Set up animation state so wallpaper widget animations are not ended on
  // their creation.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Lock screen - this is expected to start wallpaper change (e.g. to a
  // widget with a blurred wallpaper).
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);

  // If suspend starts, wallpaper animation should be stopped after screen lock
  // completes.
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  observer_->OnLockAnimationsComplete();

  WallpaperWidgetController* wallpaper_widget_controller =
      Shell::GetPrimaryRootWindowController()->wallpaper_widget_controller();
  EXPECT_FALSE(wallpaper_widget_controller->IsAnimating());

  ui::Compositor* compositor =
      Shell::GetPrimaryRootWindow()->GetHost()->compositor();
  PowerEventObserverTestApi test_api(observer_);

  // Expect that two compositing cycles are completed before suspend continues,
  // and displays get suspended.
  test_api.CompositeFrame(compositor);
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(1, GetNumVisibleCompositors());

  test_api.CompositeFrame(compositor);
  EXPECT_EQ(0, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(0, GetNumVisibleCompositors());
}

// Tests that animated wallpaper changes will be finished immediately when
// suspend starts and causes a screen lock.
TEST_F(PowerEventObserverTest, EndWallpaperAnimationAfterLockDueToSuspend) {
  chromeos::FakePowerManagerClient* client =
      chromeos::FakePowerManagerClient::Get();
  ASSERT_EQ(0, client->num_pending_suspend_readiness_callbacks());

  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);

  // Set up animation state so wallpaper widget animations are not ended on
  // their creation.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Start suspend (which should start screen lock) - verify that wallpaper is
  // not animating after the screen lock animations are reported as complete.
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  observer_->OnLockAnimationsComplete();

  WallpaperWidgetController* wallpaper_widget_controller =
      Shell::GetPrimaryRootWindowController()->wallpaper_widget_controller();
  EXPECT_FALSE(wallpaper_widget_controller->IsAnimating());

  ui::Compositor* compositor =
      Shell::GetPrimaryRootWindow()->GetHost()->compositor();
  PowerEventObserverTestApi test_api(observer_);

  // Expect that two compositing cycles are completed before suspend continues,
  // and displays get suspended.
  test_api.CompositeFrame(compositor);
  EXPECT_EQ(1, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(1, GetNumVisibleCompositors());

  test_api.CompositeFrame(compositor);
  EXPECT_EQ(0, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(0, GetNumVisibleCompositors());
}

// Tests that removing a display while power event observer is waiting for the
// wallpaper animation does not cause suspend to hang.
TEST_F(PowerEventObserverTest, DisplayRemovedDuringWallpaperAnimation) {
  chromeos::FakePowerManagerClient* client =
      chromeos::FakePowerManagerClient::Get();
  ASSERT_EQ(0, client->num_pending_suspend_readiness_callbacks());

  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);

  UpdateDisplay("200x100,300x200");

  // Set up animation state so wallpaper widget animations are not ended on
  // their creation.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Lock screen - this is expected to start wallpaper change (e.g. to a
  // widget with a blurred wallpaper).
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  observer_->OnLockAnimationsComplete();

  // Remove a display before wallpaper animation ends.
  UpdateDisplay("200x100");
  base::RunLoop().RunUntilIdle();

  // Start suspend and verify the suspend proceeds when the primary window's
  // compositors go through two compositing cycles.
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);

  ui::Compositor* compositor =
      Shell::GetPrimaryRootWindow()->GetHost()->compositor();
  PowerEventObserverTestApi test_api(observer_);

  // Expect that two compositing cycles are completed before suspend continues,
  // and displays get suspended.
  test_api.CompositeFrame(compositor);
  test_api.CompositeFrame(compositor);
  EXPECT_EQ(0, client->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(0, GetNumVisibleCompositors());
}

TEST_F(PowerEventObserverTest, LockOnLidClose) {
  // Screen should not lock if values not set.
  SetCanLockScreen(false);
  SetShouldLockScreenAutomatically(false);
  observer_->LidEventReceived(chromeos::PowerManagerClient::LidState::CLOSED,
                              base::TimeTicks::Now());
  EXPECT_FALSE(GetLockedState());

  SetCanLockScreen(false);
  SetShouldLockScreenAutomatically(true);
  observer_->LidEventReceived(chromeos::PowerManagerClient::LidState::CLOSED,
                              base::TimeTicks::Now());
  EXPECT_FALSE(GetLockedState());

  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(false);
  observer_->LidEventReceived(chromeos::PowerManagerClient::LidState::CLOSED,
                              base::TimeTicks::Now());
  EXPECT_FALSE(GetLockedState());

  // Screen should only lock on CLOSED event.
  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);
  observer_->LidEventReceived(chromeos::PowerManagerClient::LidState::OPEN,
                              base::TimeTicks::Now());
  EXPECT_FALSE(GetLockedState());
  observer_->LidEventReceived(chromeos::PowerManagerClient::LidState::CLOSED,
                              base::TimeTicks::Now());
  EXPECT_TRUE(GetLockedState());
}

TEST_F(PowerEventObserverTest, LockOnLidCloseWhenDocked) {
  std::unique_ptr<display::DisplaySnapshot> internal_display =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(gfx::Size(1024, 768))
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .Build();

  std::unique_ptr<display::DisplaySnapshot> external_display =
      display::FakeDisplaySnapshot::Builder()
          .SetId(456)
          .SetNativeMode(gfx::Size(1024, 768))
          .SetType(display::DISPLAY_CONNECTION_TYPE_VGA)
          .Build();

  auto set_docked = [&](bool docked) {
    std::vector<raw_ptr<display::DisplaySnapshot, VectorExperimental>> displays(
        {internal_display.get()});
    if (docked) {
      displays.push_back(external_display.get());
    }
    Shell::Get()->projecting_observer()->OnDisplayConfigurationChanged(
        displays);
  };

  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);

  // Closing lid should not lock when projecting.
  set_docked(true);
  observer_->LidEventReceived(chromeos::PowerManagerClient::LidState::CLOSED,
                              base::TimeTicks::Now());
  EXPECT_FALSE(GetLockedState());

  // Opening lid, then disconnect display should not lock.
  observer_->LidEventReceived(chromeos::PowerManagerClient::LidState::OPEN,
                              base::TimeTicks::Now());
  set_docked(false);
  EXPECT_FALSE(GetLockedState());

  // Closing lid while projecting, then removing display should lock.
  set_docked(true);
  observer_->LidEventReceived(chromeos::PowerManagerClient::LidState::CLOSED,
                              base::TimeTicks::Now());
  EXPECT_FALSE(GetLockedState());
  set_docked(false);
  EXPECT_TRUE(GetLockedState());
}

class LockOnSuspendUsageTest : public PowerEventObserverTest {
 public:
  LockOnSuspendUsageTest() { set_start_session(false); }
};

TEST_F(LockOnSuspendUsageTest, LockOnSuspendUsage) {
  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);

  SimulateNewUserFirstLogin("user@gmail.com");
  PowerEventObserverTestApi test_api(observer_);
  ASSERT_TRUE(test_api.TrackingLockOnSuspendUsage());

  base::HistogramTester histogram_tester;
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("ChromeOS.FeatureUsage.LockOnSuspend"),
      ::testing::ElementsAre(
          base::Bucket(
              static_cast<int>(
                  feature_usage::FeatureUsageMetrics::Event::kEligible),
              1),
          base::Bucket(static_cast<int>(
                           feature_usage::FeatureUsageMetrics::Event::kEnabled),
                       1),
          base::Bucket(
              static_cast<int>(
                  feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
              1)));
}

// TODO(crbug.com/40898491): Test is failing on "Linux ChromiumOS MSan Tests".
#if defined(MEMORY_SANITIZER)
#define MAYBE_No_ShouldLockScreenAutomatically \
  DISABLED_No_ShouldLockScreenAutomatically
#else
#define MAYBE_No_ShouldLockScreenAutomatically No_ShouldLockScreenAutomatically
#endif
TEST_F(LockOnSuspendUsageTest, MAYBE_No_ShouldLockScreenAutomatically) {
  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(false);

  SimulateNewUserFirstLogin("user@gmail.com");
  PowerEventObserverTestApi test_api(observer_);
  ASSERT_TRUE(test_api.TrackingLockOnSuspendUsage());

  base::HistogramTester histogram_tester;
  observer_->SuspendImminent(power_manager::SuspendImminent_Reason_OTHER);
  histogram_tester.ExpectTotalCount("ChromeOS.FeatureUsage.LockOnSuspend", 0);
}

TEST_F(LockOnSuspendUsageTest, No_CanLockScreen) {
  SetCanLockScreen(false);
  SimulateNewUserFirstLogin("user@gmail.com");
  PowerEventObserverTestApi test_api(observer_);
  ASSERT_FALSE(test_api.TrackingLockOnSuspendUsage());
}

}  // namespace ash
