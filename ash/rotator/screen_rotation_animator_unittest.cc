// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rotator/screen_rotation_animator.h"

#include <memory>

#include "ash/display/display_configuration_controller_test_api.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/rotator/screen_rotation_animator_observer.h"
#include "ash/rotator/screen_rotation_animator_test_api.h"
#include "ash/shell.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

display::Display::Rotation GetDisplayRotation(int64_t display_id) {
  return Shell::Get()
      ->display_manager()
      ->GetDisplayInfo(display_id)
      .GetActiveRotation();
}

void SetDisplayRotation(int64_t display_id,
                        display::Display::Rotation rotation) {
  Shell::Get()->display_manager()->SetDisplayRotation(
      display_id, rotation, display::Display::RotationSource::USER);
}

OverviewButtonTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
      ->overview_button_tray();
}

class AnimationObserver : public ScreenRotationAnimatorObserver {
 public:
  AnimationObserver() = default;

  bool notified() const { return notified_; }

  void OnScreenRotationAnimationFinished(
      ScreenRotationAnimator* animator) override {
    notified_ = true;
  }

 private:
  bool notified_ = false;

  DISALLOW_COPY_AND_ASSIGN(AnimationObserver);
};

class TestScreenRotationAnimator : public ScreenRotationAnimator {
 public:
  TestScreenRotationAnimator(aura::Window* root_window,
                             const base::Closure& before_callback,
                             const base::Closure& after_callback);
  ~TestScreenRotationAnimator() override = default;

 private:
  CopyCallback CreateAfterCopyCallbackBeforeRotation(
      std::unique_ptr<ScreenRotationRequest> rotation_request) override;
  CopyCallback CreateAfterCopyCallbackAfterRotation(
      std::unique_ptr<ScreenRotationRequest> rotation_request) override;

  void IntersectBefore(CopyCallback next_callback,
                       std::unique_ptr<viz::CopyOutputResult> result);
  void IntersectAfter(CopyCallback next_callback,
                      std::unique_ptr<viz::CopyOutputResult> result);

  base::Closure intersect_before_callback_;
  base::Closure intersect_after_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestScreenRotationAnimator);
};

TestScreenRotationAnimator::TestScreenRotationAnimator(
    aura::Window* root_window,
    const base::Closure& before_callback,
    const base::Closure& after_callback)
    : ScreenRotationAnimator(root_window),
      intersect_before_callback_(before_callback),
      intersect_after_callback_(after_callback) {}

ScreenRotationAnimator::CopyCallback
TestScreenRotationAnimator::CreateAfterCopyCallbackBeforeRotation(
    std::unique_ptr<ScreenRotationRequest> rotation_request) {
  CopyCallback next_callback =
      ScreenRotationAnimator::CreateAfterCopyCallbackBeforeRotation(
          std::move(rotation_request));
  return base::BindOnce(&TestScreenRotationAnimator::IntersectBefore,
                        base::Unretained(this), std::move(next_callback));
}

ScreenRotationAnimator::CopyCallback
TestScreenRotationAnimator::CreateAfterCopyCallbackAfterRotation(
    std::unique_ptr<ScreenRotationRequest> rotation_request) {
  CopyCallback next_callback =
      ScreenRotationAnimator::CreateAfterCopyCallbackAfterRotation(
          std::move(rotation_request));
  return base::BindOnce(&TestScreenRotationAnimator::IntersectAfter,
                        base::Unretained(this), std::move(next_callback));
}

void TestScreenRotationAnimator::IntersectBefore(
    CopyCallback next_callback,
    std::unique_ptr<viz::CopyOutputResult> result) {
  intersect_before_callback_.Run();
  std::move(next_callback).Run(std::move(result));
}

void TestScreenRotationAnimator::IntersectAfter(
    CopyCallback next_callback,
    std::unique_ptr<viz::CopyOutputResult> result) {
  intersect_after_callback_.Run();
  std::move(next_callback).Run(std::move(result));
}

}  // namespace

class ScreenRotationAnimatorSlowAnimationTest : public AshTestBase {
 public:
  ScreenRotationAnimatorSlowAnimationTest() = default;
  ~ScreenRotationAnimatorSlowAnimationTest() override = default;

  // AshTestBase:
  void SetUp() override;

 protected:
  int64_t display_id() const { return display_.id(); }

  ScreenRotationAnimator* animator() { return animator_.get(); }

  ScreenRotationAnimatorTestApi* test_api() { return test_api_.get(); }

 private:
  display::Display display_;

  std::unique_ptr<ScreenRotationAnimator> animator_;

  std::unique_ptr<ScreenRotationAnimatorTestApi> test_api_;

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> non_zero_duration_mode_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotationAnimatorSlowAnimationTest);
};

void ScreenRotationAnimatorSlowAnimationTest::SetUp() {
  AshTestBase::SetUp();

  display_ = display::Screen::GetScreen()->GetPrimaryDisplay();
  animator_ = std::make_unique<ScreenRotationAnimator>(
      Shell::GetRootWindowForDisplayId(display_.id()));
  test_api_ = std::make_unique<ScreenRotationAnimatorTestApi>(animator_.get());
  test_api()->DisableAnimationTimers();
  non_zero_duration_mode_ =
      std::make_unique<ui::ScopedAnimationDurationScaleMode>(
          ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
}

class ScreenRotationAnimatorSmoothAnimationTest : public AshTestBase {
 public:
  ScreenRotationAnimatorSmoothAnimationTest() = default;
  ~ScreenRotationAnimatorSmoothAnimationTest() override = default;

  // AshTestBase:
  void SetUp() override;

  void RemoveSecondaryDisplay(const std::string& specs);
  void QuitWaitForCopyCallback();

 protected:
  int64_t display_id() const { return display_.id(); }

  TestScreenRotationAnimator* animator() { return animator_.get(); }

  void SetScreenRotationAnimator(aura::Window* root_window,
                                 const base::Closure& before_callback,
                                 const base::Closure& after_callback);

  ScreenRotationAnimatorTestApi* test_api() { return test_api_.get(); }

  void WaitForCopyCallback();

  std::unique_ptr<base::RunLoop> run_loop_;

 protected:
  std::unique_ptr<TestScreenRotationAnimator> animator_;

 private:
  display::Display display_;

  std::unique_ptr<ScreenRotationAnimatorTestApi> test_api_;

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> non_zero_duration_mode_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotationAnimatorSmoothAnimationTest);
};

void ScreenRotationAnimatorSmoothAnimationTest::RemoveSecondaryDisplay(
    const std::string& specs) {
  UpdateDisplay(specs);
  QuitWaitForCopyCallback();
}

void ScreenRotationAnimatorSmoothAnimationTest::QuitWaitForCopyCallback() {
  run_loop_->QuitWhenIdle();
}

void ScreenRotationAnimatorSmoothAnimationTest::SetUp() {
  AshTestBase::SetUp();
  // Resets the commandline will clear all the switches, including
  // "ash-disable-smooth-screen-rotation", so that we can test the smooth screen
  // rotation animation. The |animator| is recreated and checking this swtich.
  ash_test_helper()->reset_commandline();

  display_ = display::Screen::GetScreen()->GetPrimaryDisplay();
  run_loop_ = std::make_unique<base::RunLoop>();
  SetScreenRotationAnimator(Shell::GetRootWindowForDisplayId(display_.id()),
                            run_loop_->QuitWhenIdleClosure(),
                            run_loop_->QuitWhenIdleClosure());
  non_zero_duration_mode_ =
      std::make_unique<ui::ScopedAnimationDurationScaleMode>(
          ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
}

void ScreenRotationAnimatorSmoothAnimationTest::SetScreenRotationAnimator(
    aura::Window* root_window,
    const base::Closure& before_callback,
    const base::Closure& after_callback) {
  animator_ = std::make_unique<TestScreenRotationAnimator>(
      root_window, before_callback, after_callback);
  test_api_ = std::make_unique<ScreenRotationAnimatorTestApi>(animator_.get());
  test_api()->DisableAnimationTimers();
}

void ScreenRotationAnimatorSmoothAnimationTest::WaitForCopyCallback() {
  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();
}

TEST_F(ScreenRotationAnimatorSlowAnimationTest, ShouldNotifyObserver) {
  SetDisplayRotation(display_id(), display::Display::ROTATE_0);
  AnimationObserver observer;
  animator()->AddScreenRotationAnimatorObserver(&observer);
  EXPECT_FALSE(observer.notified());

  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_SYNC);
  EXPECT_FALSE(observer.notified());

  test_api()->CompleteAnimations();
  EXPECT_TRUE(observer.notified());
  EXPECT_FALSE(test_api()->HasActiveAnimations());
  animator()->RemoveScreenRotationAnimatorObserver(&observer);
}

TEST_F(ScreenRotationAnimatorSlowAnimationTest, ShouldNotifyObserverOnce) {
  SetDisplayRotation(display_id(), display::Display::ROTATE_0);
  AnimationObserver observer;
  animator()->AddScreenRotationAnimatorObserver(&observer);
  EXPECT_FALSE(observer.notified());

  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_SYNC);
  EXPECT_FALSE(observer.notified());

  animator()->Rotate(display::Display::ROTATE_180,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_SYNC);
  EXPECT_FALSE(observer.notified());

  test_api()->CompleteAnimations();
  EXPECT_TRUE(observer.notified());
  EXPECT_FALSE(test_api()->HasActiveAnimations());
  animator()->RemoveScreenRotationAnimatorObserver(&observer);
}

TEST_F(ScreenRotationAnimatorSlowAnimationTest, RotatesToDifferentRotation) {
  SetDisplayRotation(display_id(), display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_SYNC);
  EXPECT_TRUE(test_api()->HasActiveAnimations());

  test_api()->CompleteAnimations();
  EXPECT_FALSE(test_api()->HasActiveAnimations());
}

TEST_F(ScreenRotationAnimatorSlowAnimationTest,
       ShouldNotRotateTheSameRotation) {
  SetDisplayRotation(display_id(), display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_0,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_SYNC);
  EXPECT_FALSE(test_api()->HasActiveAnimations());
}

// Simulates the situation that if there is a new rotation request during
// animation, it should stop the animation immediately and add the new rotation
// request to the |last_pending_request_|.
TEST_F(ScreenRotationAnimatorSlowAnimationTest, RotatesDuringRotation) {
  SetDisplayRotation(display_id(), display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_SYNC);
  EXPECT_TRUE(animator()->IsRotating());
  EXPECT_EQ(display::Display::ROTATE_90, animator()->GetTargetRotation());

  animator()->Rotate(display::Display::ROTATE_180,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_SYNC);
  EXPECT_TRUE(test_api()->HasActiveAnimations());
  EXPECT_TRUE(animator()->IsRotating());
  EXPECT_EQ(display::Display::ROTATE_180, animator()->GetTargetRotation());

  test_api()->CompleteAnimations();
  EXPECT_FALSE(test_api()->HasActiveAnimations());
  EXPECT_FALSE(animator()->IsRotating());

  EXPECT_EQ(display::Display::ROTATE_180, GetDisplayRotation(display_id()));
}

// If there are multiple requests queued during animation, it should process the
// last request and finish the rotation animation.
TEST_F(ScreenRotationAnimatorSlowAnimationTest, ShouldCompleteAnimations) {
  SetDisplayRotation(display_id(), display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_SYNC);
  EXPECT_TRUE(test_api()->HasActiveAnimations());

  animator()->Rotate(display::Display::ROTATE_180,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_SYNC);
  EXPECT_TRUE(test_api()->HasActiveAnimations());

  animator()->Rotate(display::Display::ROTATE_270,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_SYNC);
  EXPECT_TRUE(test_api()->HasActiveAnimations());

  test_api()->CompleteAnimations();
  EXPECT_FALSE(test_api()->HasActiveAnimations());
  EXPECT_EQ(display::Display::ROTATE_270, GetDisplayRotation(display_id()));
}

// Test that slow screen rotation animation will not interrupt hide animation.
// The OverviewButton should be hidden.
TEST_F(ScreenRotationAnimatorSlowAnimationTest,
       OverviewButtonTrayHideAnimationAlwaysCompletes) {
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  // Long duration for hide animation, to allow it to be interrupted.
  ui::ScopedAnimationDurationScaleMode hide_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  GetTray()->SetVisible(false);

  // ScreenRotationAnimator copies the current layers, and deletes them upon
  // completion. Allow its animation to complete first.
  ui::ScopedAnimationDurationScaleMode rotate_duration(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  SetDisplayRotation(display_id(), display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_SYNC);

  EXPECT_FALSE(GetTray()->visible());
}

// Test enable smooth screen rotation code path.
TEST_F(ScreenRotationAnimatorSmoothAnimationTest,
       RotatesToDifferentRotationWithCopyCallback) {
  const int64_t display_id = display_manager()->GetDisplayAt(0).id();
  SetScreenRotationAnimator(
      Shell::GetRootWindowForDisplayId(display_id),
      run_loop_->QuitWhenIdleClosure(),
      base::Bind(
          &ScreenRotationAnimatorSmoothAnimationTest::QuitWaitForCopyCallback,
          base::Unretained(this)));
  SetDisplayRotation(display_id, display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_ASYNC);
  EXPECT_TRUE(animator()->IsRotating());

  EXPECT_EQ(display::Display::ROTATE_90, animator()->GetTargetRotation());
  EXPECT_NE(display::Display::ROTATE_90, GetDisplayRotation(display_id));

  WaitForCopyCallback();
  EXPECT_TRUE(test_api()->HasActiveAnimations());
  EXPECT_EQ(display::Display::ROTATE_90, animator()->GetTargetRotation());
  // Once copy is made, the rotation is set to the target, with the
  // image that was rotated to the original orientation.
  EXPECT_EQ(display::Display::ROTATE_90, GetDisplayRotation(display_id));

  test_api()->CompleteAnimations();
  EXPECT_FALSE(test_api()->HasActiveAnimations());
  EXPECT_EQ(display::Display::ROTATE_90, GetDisplayRotation(display_id));
}

// If the rotating external secondary display is removed before the first copy
// request callback called, it should stop rotating.
TEST_F(ScreenRotationAnimatorSmoothAnimationTest,
       RemoveExternalSecondaryDisplayBeforeFirstCopyCallback) {
  UpdateDisplay("640x480,800x600");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());

  const int64_t primary_display_id = display_manager()->GetDisplayAt(0).id();
  const int64_t secondary_display_id = display_manager()->GetDisplayAt(1).id();

  SetScreenRotationAnimator(
      Shell::GetRootWindowForDisplayId(secondary_display_id),
      base::Bind(
          &ScreenRotationAnimatorSmoothAnimationTest::RemoveSecondaryDisplay,
          base::Unretained(this), "640x480"),
      run_loop_->QuitWhenIdleClosure());
  SetDisplayRotation(secondary_display_id, display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_ASYNC);
  WaitForCopyCallback();
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(primary_display_id, display_manager()->GetDisplayAt(0).id());
}

// If the rotating external primary display is removed before the first copy
// request callback called, it should stop rotating.
TEST_F(ScreenRotationAnimatorSmoothAnimationTest,
       RemoveExternalPrimaryDisplayBeforeFirstCopyCallback) {
  UpdateDisplay("640x480,800x600");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());

  Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(
      display_manager()->GetDisplayAt(1).id());
  const int64_t primary_display_id = display_manager()->GetDisplayAt(1).id();
  const int64_t secondary_display_id = display_manager()->GetDisplayAt(0).id();
  SetScreenRotationAnimator(
      Shell::GetRootWindowForDisplayId(primary_display_id),
      base::Bind(
          &ScreenRotationAnimatorSmoothAnimationTest::RemoveSecondaryDisplay,
          base::Unretained(this), "640x480"),
      run_loop_->QuitWhenIdleClosure());
  SetDisplayRotation(primary_display_id, display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_ASYNC);
  WaitForCopyCallback();
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(secondary_display_id, display_manager()->GetDisplayAt(0).id());
}

// If the rotating external secondary display is removed before the second copy
// request callback called, it should stop rotating.
TEST_F(ScreenRotationAnimatorSmoothAnimationTest,
       RemoveExternalSecondaryDisplayBeforeSecondCopyCallback) {
  UpdateDisplay("640x480,800x600");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());

  const int64_t primary_display_id = display_manager()->GetDisplayAt(0).id();
  const int64_t secondary_display_id = display_manager()->GetDisplayAt(1).id();
  SetScreenRotationAnimator(
      Shell::GetRootWindowForDisplayId(secondary_display_id),
      run_loop_->QuitWhenIdleClosure(),
      base::Bind(
          &ScreenRotationAnimatorSmoothAnimationTest::RemoveSecondaryDisplay,
          base::Unretained(this), "640x480"));
  SetDisplayRotation(secondary_display_id, display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_ASYNC);
  WaitForCopyCallback();
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(primary_display_id, display_manager()->GetDisplayAt(0).id());
}

// If the rotating external primary display is removed before the second copy
// request callback called, it should stop rotating.
TEST_F(ScreenRotationAnimatorSmoothAnimationTest,
       RemoveExternalPrimaryDisplayBeforeSecondCopyCallback) {
  UpdateDisplay("640x480,800x600");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());

  Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(
      display_manager()->GetDisplayAt(1).id());
  const int64_t primary_display_id = display_manager()->GetDisplayAt(1).id();
  const int64_t secondary_display_id = display_manager()->GetDisplayAt(0).id();
  SetScreenRotationAnimator(
      Shell::GetRootWindowForDisplayId(primary_display_id),
      run_loop_->QuitWhenIdleClosure(),
      base::Bind(
          &ScreenRotationAnimatorSmoothAnimationTest::RemoveSecondaryDisplay,
          base::Unretained(this), "640x480"));
  SetDisplayRotation(primary_display_id, display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_ASYNC);
  WaitForCopyCallback();
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(secondary_display_id, display_manager()->GetDisplayAt(0).id());
}

// If the external primary display is removed while rotating the secondary
// display. It should stop rotating the secondary display because the
// |root_window| changed.
TEST_F(ScreenRotationAnimatorSmoothAnimationTest,
       RemoveExternalPrimaryDisplayDuringAnimationChangedRootWindow) {
  UpdateDisplay("640x480,800x600");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());

  Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(
      display_manager()->GetDisplayAt(1).id());
  const int64_t secondary_display_id = display_manager()->GetDisplayAt(0).id();
  SetScreenRotationAnimator(
      Shell::GetRootWindowForDisplayId(secondary_display_id),
      base::Bind(
          &ScreenRotationAnimatorSmoothAnimationTest::RemoveSecondaryDisplay,
          base::Unretained(this), "640x480"),
      run_loop_->QuitWhenIdleClosure());
  SetDisplayRotation(secondary_display_id, display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_ASYNC);
  WaitForCopyCallback();
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(secondary_display_id, display_manager()->GetDisplayAt(0).id());
  EXPECT_EQ(display::Display::ROTATE_0,
            GetDisplayRotation(secondary_display_id));
}

// Test that smooth screen rotation animation will not interrupt hide animation.
// The OverviewButton should be hidden.
TEST_F(ScreenRotationAnimatorSmoothAnimationTest,
       OverviewButtonTrayHideAnimationAlwaysCompletes) {
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  // Long duration for hide animation, to allow it to be interrupted.
  ui::ScopedAnimationDurationScaleMode hide_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  GetTray()->SetVisible(false);

  // Allow ScreenRotationAnimator animation to complete first.
  ui::ScopedAnimationDurationScaleMode rotate_duration(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  int64_t display_id = display_manager()->GetDisplayAt(0).id();
  SetScreenRotationAnimator(
      Shell::GetRootWindowForDisplayId(display_id),
      run_loop_->QuitWhenIdleClosure(),
      base::Bind(
          &ScreenRotationAnimatorSmoothAnimationTest::QuitWaitForCopyCallback,
          base::Unretained(this)));
  SetDisplayRotation(display_id, display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_ASYNC);
  WaitForCopyCallback();

  GetTray()->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(GetTray()->visible());
}

// Test that smooth screen rotation animation will work when |root_window|
// recreated.
TEST_F(ScreenRotationAnimatorSmoothAnimationTest,
       ShouldRotateAfterRecreateLayers) {
  const int64_t display_id = display_manager()->GetDisplayAt(0).id();
  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display_id);
  SetScreenRotationAnimator(
      root_window, run_loop_->QuitWhenIdleClosure(),
      base::Bind(
          &ScreenRotationAnimatorSmoothAnimationTest::QuitWaitForCopyCallback,
          base::Unretained(this)));
  SetDisplayRotation(display_id, display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_ASYNC);
  WaitForCopyCallback();
  EXPECT_TRUE(test_api()->HasActiveAnimations());

  test_api()->CompleteAnimations();
  EXPECT_FALSE(test_api()->HasActiveAnimations());
  EXPECT_EQ(display::Display::ROTATE_90, GetDisplayRotation(display_id));

  // Colone and delete the old layer tree.
  std::unique_ptr<ui::LayerTreeOwner> old_layer_tree_owner =
      ::wm::RecreateLayers(root_window);
  old_layer_tree_owner.reset();

  // Should work for another rotation.
  animator()->Rotate(display::Display::ROTATE_180,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_ASYNC);
  WaitForCopyCallback();
  EXPECT_TRUE(test_api()->HasActiveAnimations());

  test_api()->CompleteAnimations();
  EXPECT_FALSE(test_api()->HasActiveAnimations());
  EXPECT_EQ(display::Display::ROTATE_180, GetDisplayRotation(display_id));
}

TEST_F(ScreenRotationAnimatorSmoothAnimationTest, DisplayChangeDuringCopy) {
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  aura::Window* root_window =
      Shell::GetRootWindowForDisplayId(internal_display_id);
  SetScreenRotationAnimator(
      root_window,
      base::Bind(
          &ScreenRotationAnimatorSmoothAnimationTest::QuitWaitForCopyCallback,
          base::Unretained(this)),
      run_loop_->QuitWhenIdleClosure());

  TestScreenRotationAnimator* animator = animator_.get();
  DisplayConfigurationControllerTestApi testapi(
      Shell::Get()->display_configuration_controller());
  testapi.SetDisplayAnimator(true);
  testapi.SetScreenRotationAnimatorForDisplay(internal_display_id,
                                              std::move(animator_));
  ScreenOrientationControllerTestApi(
      Shell::Get()->screen_orientation_controller())
      .SetDisplayRotation(display::Display::ROTATE_90,
                          display::Display::RotationSource::ACCELEROMETER);

  EXPECT_TRUE(animator->IsRotating());
  display_manager()->UpdateDisplays();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  EXPECT_FALSE(animator->IsRotating());

  WaitForCopyCallback();
  EXPECT_FALSE(animator->IsRotating());
  EXPECT_EQ(display::Display::ROTATE_0,
            GetDisplayRotation(internal_display_id));
}

TEST_F(ScreenRotationAnimatorSmoothAnimationTest, NewRequestShouldNotCancel) {
  const int64_t display_id = display_manager()->GetDisplayAt(0).id();
  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display_id);
  SetScreenRotationAnimator(
      root_window, run_loop_->QuitWhenIdleClosure(),
      base::BindRepeating(
          &ScreenRotationAnimatorSmoothAnimationTest::QuitWaitForCopyCallback,
          base::Unretained(this)));
  SetDisplayRotation(display_id, display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_ASYNC);
  EXPECT_EQ(display::Display::ROTATE_0, GetDisplayRotation(display_id));

  // Requesting new orientation while waiting for copy should apply the previous
  // change immediately.
  animator()->Rotate(display::Display::ROTATE_180,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_ASYNC);
  EXPECT_EQ(display::Display::ROTATE_90, GetDisplayRotation(display_id));

  // Requesting yet another new orientation while waiting for copy should do the
  // same.
  animator()->Rotate(display::Display::ROTATE_270,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_ASYNC);
  EXPECT_EQ(display::Display::ROTATE_180, GetDisplayRotation(display_id));

  WaitForCopyCallback();
  // The display must be rotated once copy finishes.
  EXPECT_EQ(display::Display::ROTATE_270, GetDisplayRotation(display_id));
  EXPECT_TRUE(test_api()->HasActiveAnimations());

  // Requesting new orientation while animating will be queued.
  animator()->Rotate(display::Display::ROTATE_0,
                     display::Display::RotationSource::USER,
                     DisplayConfigurationController::ANIMATION_ASYNC);
  EXPECT_EQ(display::Display::ROTATE_270, GetDisplayRotation(display_id));
  EXPECT_FALSE(test_api()->HasActiveAnimations());

  // Finish current animation will start queued animation (from 270 to 0).
  test_api()->CompleteAnimations();
  EXPECT_TRUE(animator()->IsRotating());
  EXPECT_EQ(display::Display::ROTATE_270, GetDisplayRotation(display_id));
  EXPECT_FALSE(test_api()->HasActiveAnimations());

  WaitForCopyCallback();
  EXPECT_TRUE(test_api()->HasActiveAnimations());
  EXPECT_EQ(display::Display::ROTATE_0, GetDisplayRotation(display_id));

  test_api()->CompleteAnimations();
  EXPECT_FALSE(test_api()->HasActiveAnimations());
  EXPECT_EQ(display::Display::ROTATE_0, GetDisplayRotation(display_id));
}

}  // namespace ash
