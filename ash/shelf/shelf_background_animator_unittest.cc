// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_background_animator.h"

#include <memory>

#include "ash/animation/animation_change_type.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/slide_animation.h"

namespace ash {
namespace {

// A valid color value that is distinct from any final animation state values.
// Used to check if color values are changed during animations.
const SkColor kDummyColor = SK_ColorBLUE;

// Observer that caches color values for the last observation. This observer
// will also call a set callback when ShelfBackgroundAnimator completes an
// animation.
class TestShelfBackgroundObserver : public ShelfBackgroundAnimatorObserver {
 public:
  TestShelfBackgroundObserver() = default;

  TestShelfBackgroundObserver(const TestShelfBackgroundObserver&) = delete;
  TestShelfBackgroundObserver& operator=(const TestShelfBackgroundObserver&) =
      delete;

  ~TestShelfBackgroundObserver() override = default;

  SkColor background_color() const { return background_color_; }

  // Convenience function to get the alpha value from |background_color_|.
  int GetBackgroundAlpha() const;

  // Sets |animation_complete_callback_| to be called when animation ends.
  void SetAnimationCompleteCallback(base::OnceClosure callback);

  // ShelfBackgroundObserver:
  void UpdateShelfBackground(SkColor color) override;
  void OnShelfBackgroundAnimationEnded() override;

 private:
  int background_color_ = SK_ColorTRANSPARENT;

  base::OnceClosure animation_complete_callback_;
};

int TestShelfBackgroundObserver::GetBackgroundAlpha() const {
  return SkColorGetA(background_color_);
}

void TestShelfBackgroundObserver::UpdateShelfBackground(SkColor color) {
  background_color_ = color;
}

void TestShelfBackgroundObserver::OnShelfBackgroundAnimationEnded() {
  if (!animation_complete_callback_.is_null())
    std::move(animation_complete_callback_).Run();
}

void TestShelfBackgroundObserver::SetAnimationCompleteCallback(
    base::OnceClosure callback) {
  animation_complete_callback_ = std::move(callback);
}

}  // namespace

// Provides internal access to a ShelfBackgroundAnimator instance.
class ShelfBackgroundAnimatorTestApi {
 public:
  explicit ShelfBackgroundAnimatorTestApi(ShelfBackgroundAnimator* animator)
      : animator_(animator) {}

  ShelfBackgroundAnimatorTestApi(const ShelfBackgroundAnimatorTestApi&) =
      delete;
  ShelfBackgroundAnimatorTestApi& operator=(
      const ShelfBackgroundAnimatorTestApi&) = delete;

  ~ShelfBackgroundAnimatorTestApi() = default;

  ShelfBackgroundType previous_background_type() const {
    return animator_->previous_background_type_;
  }

  gfx::SlideAnimation* animator() const { return animator_->animator_.get(); }

  SkColor shelf_background_target_color() const {
    return animator_->shelf_background_values_.target_color();
  }

 private:
  // The instance to provide internal access to.
  raw_ptr<ShelfBackgroundAnimator, DanglingUntriaged> animator_;
};

class ShelfBackgroundAnimatorTest : public AshTestBase {
 public:
  ShelfBackgroundAnimatorTest() = default;

  ShelfBackgroundAnimatorTest(const ShelfBackgroundAnimatorTest&) = delete;
  ShelfBackgroundAnimatorTest& operator=(const ShelfBackgroundAnimatorTest&) =
      delete;

  ~ShelfBackgroundAnimatorTest() override = default;

  // testing::Test:
  void SetUp() override;

 protected:
  // Convenience wrapper for |animator_|'s PaintBackground().
  void PaintBackground(
      ShelfBackgroundType background_type,
      AnimationChangeType change_type = AnimationChangeType::IMMEDIATE);

  // Set all of the color values for the |observer_|.
  void SetColorValuesOnObserver(SkColor color);

  // Waits for animation to complete.
  void WaitForAnimationCompletion();

  TestShelfBackgroundObserver observer_;

  // Test target.
  raw_ptr<ShelfBackgroundAnimator, DanglingUntriaged> animator_;

  // Provides internal access to |animator_|.
  std::unique_ptr<ShelfBackgroundAnimatorTestApi> test_api_;
};

void ShelfBackgroundAnimatorTest::SetUp() {
  AshTestBase::SetUp();

  animator_ =
      GetPrimaryShelf()->shelf_widget()->background_animator_for_testing();
  animator_->AddObserver(&observer_);

  test_api_ = std::make_unique<ShelfBackgroundAnimatorTestApi>(animator_);
}

void ShelfBackgroundAnimatorTest::PaintBackground(
    ShelfBackgroundType background_type,
    AnimationChangeType change_type) {
  animator_->PaintBackground(background_type, change_type);
}

void ShelfBackgroundAnimatorTest::SetColorValuesOnObserver(SkColor color) {
  observer_.UpdateShelfBackground(color);
}

void ShelfBackgroundAnimatorTest::WaitForAnimationCompletion() {
  base::RunLoop run_loop;

  observer_.SetAnimationCompleteCallback(run_loop.QuitWhenIdleClosure());

  run_loop.Run();
}

// Verify the |previous_background_type_| and |target_background_type_| values
// when animating to the same target type multiple times.
TEST_F(ShelfBackgroundAnimatorTest, BackgroundTypesWhenAnimatingToSameTarget) {
  PaintBackground(ShelfBackgroundType::kMaximized);
  EXPECT_EQ(ShelfBackgroundType::kMaximized,
            animator_->target_background_type());

  PaintBackground(ShelfBackgroundType::kDefaultBg);
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            animator_->target_background_type());
  EXPECT_EQ(ShelfBackgroundType::kMaximized,
            test_api_->previous_background_type());

  PaintBackground(ShelfBackgroundType::kDefaultBg);
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            animator_->target_background_type());
  EXPECT_EQ(ShelfBackgroundType::kMaximized,
            test_api_->previous_background_type());
}

// Verify subsequent calls to PaintBackground() using the
// AnimationChangeType::ANIMATE change type are ignored.
TEST_F(ShelfBackgroundAnimatorTest,
       MultipleAnimateCallsToSameTargetAreIgnored) {
  PaintBackground(ShelfBackgroundType::kMaximized);
  SetColorValuesOnObserver(kDummyColor);

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  animator_->PaintBackground(ShelfBackgroundType::kDefaultBg,
                             AnimationChangeType::ANIMATE);
  WaitForAnimationCompletion();

  EXPECT_NE(observer_.background_color(), kDummyColor);

  SetColorValuesOnObserver(kDummyColor);
  animator_->PaintBackground(ShelfBackgroundType::kDefaultBg,
                             AnimationChangeType::ANIMATE);

  EXPECT_EQ(observer_.background_color(), kDummyColor);
}

// Verify observers are updated with the current values when they are added.
TEST_F(ShelfBackgroundAnimatorTest, ObserversUpdatedWhenAdded) {
  animator_->RemoveObserver(&observer_);
  SetColorValuesOnObserver(kDummyColor);

  animator_->AddObserver(&observer_);

  EXPECT_NE(observer_.background_color(), kDummyColor);
}

// Verify the alpha values for the ShelfBackgroundType::kDefaultBg state.
TEST_F(ShelfBackgroundAnimatorTest, DefaultBackground) {
  PaintBackground(ShelfBackgroundType::kDefaultBg);

  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            animator_->target_background_type());
  EXPECT_EQ((int)SkColorGetA(ShelfConfig::Get()->GetDefaultShelfColor(
                GetPrimaryShelf()->shelf_widget())),
            observer_.GetBackgroundAlpha());
}

// Verify the alpha values for the ShelfBackgroundType::kMaximized state.
TEST_F(ShelfBackgroundAnimatorTest, MaximizedBackground) {
  PaintBackground(ShelfBackgroundType::kMaximized);

  EXPECT_EQ(ShelfBackgroundType::kMaximized,
            animator_->target_background_type());
  EXPECT_EQ((int)SkColorGetA(ShelfConfig::Get()->GetMaximizedShelfColor(
                GetPrimaryShelf()->shelf_widget())),
            observer_.GetBackgroundAlpha());
}

TEST_F(ShelfBackgroundAnimatorTest,
       AnimatorIsDetroyedWhenCompletingSuccessfully) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  PaintBackground(ShelfBackgroundType::kMaximized,
                  AnimationChangeType::ANIMATE);
  EXPECT_TRUE(test_api_->animator());
  WaitForAnimationCompletion();

  EXPECT_FALSE(test_api_->animator());
}

TEST_F(ShelfBackgroundAnimatorTest,
       AnimatorDestroyedWhenChangingBackgroundImmediately) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  PaintBackground(ShelfBackgroundType::kMaximized,
                  AnimationChangeType::ANIMATE);
  EXPECT_TRUE(test_api_->animator());

  PaintBackground(ShelfBackgroundType::kDefaultBg,
                  AnimationChangeType::IMMEDIATE);
  EXPECT_FALSE(test_api_->animator());
}

// Verify that existing animator is used when animating to the previous state.
TEST_F(ShelfBackgroundAnimatorTest,
       ExistingAnimatorIsReusedWhenAnimatingToPreviousState) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // First PaintBackground() must be immediate so that the
  // ShelfBackgroundAnimator has its color set correctly.
  PaintBackground(ShelfBackgroundType::kDefaultBg,
                  AnimationChangeType::IMMEDIATE);
  PaintBackground(ShelfBackgroundType::kMaximized,
                  AnimationChangeType::ANIMATE);

  const gfx::SlideAnimation* animator = test_api_->animator();
  EXPECT_TRUE(animator);

  PaintBackground(ShelfBackgroundType::kDefaultBg,
                  AnimationChangeType::ANIMATE);

  EXPECT_EQ(animator, test_api_->animator());
}

// Verify that existing animator is not re-used when the target background isn't
// the same as the previous background.
TEST_F(ShelfBackgroundAnimatorTest,
       ExistingAnimatorNotReusedWhenTargetBackgroundNotPreviousBackground) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  PaintBackground(ShelfBackgroundType::kHomeLauncher,
                  AnimationChangeType::ANIMATE);

  const gfx::SlideAnimation* animator = test_api_->animator();
  EXPECT_TRUE(animator);

  EXPECT_NE(ShelfBackgroundType::kMaximized,
            test_api_->previous_background_type());
  PaintBackground(ShelfBackgroundType::kMaximized,
                  AnimationChangeType::ANIMATE);

  EXPECT_NE(animator, test_api_->animator());
}

// Verify observers are always notified, even when alpha values don't change.
TEST_F(ShelfBackgroundAnimatorTest,
       ObserversAreNotifiedWhenSnappingToSameTargetBackground) {
  PaintBackground(ShelfBackgroundType::kDefaultBg);
  SetColorValuesOnObserver(kDummyColor);
  PaintBackground(ShelfBackgroundType::kDefaultBg);

  EXPECT_NE(observer_.background_color(), kDummyColor);
}

class ShelfBackgroundTargetColorTest : public NoSessionAshTestBase {
 public:
  ShelfBackgroundTargetColorTest() = default;

  ShelfBackgroundTargetColorTest(const ShelfBackgroundTargetColorTest&) =
      delete;
  ShelfBackgroundTargetColorTest& operator=(
      const ShelfBackgroundTargetColorTest&) = delete;

  ~ShelfBackgroundTargetColorTest() override = default;

 protected:
  // Helper function to notify session state changes.
  void NotifySessionStateChanged(session_manager::SessionState state) {
    GetSessionControllerClient()->SetSessionState(state);
  }
};

// Verify the target color of the shelf background is updated based on session
// state, starting from LOGIN_PRIMARY.
TEST_F(ShelfBackgroundTargetColorTest, ShelfBackgroundColorUpdatedFromLogin) {
  ShelfBackgroundAnimatorTestApi test_api(
      Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
          ->shelf_widget()
          ->background_animator_for_testing());

  NotifySessionStateChanged(session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(test_api.shelf_background_target_color(), SK_ColorTRANSPARENT);

  SimulateUserLogin("user1@test.com");

  NotifySessionStateChanged(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(test_api.shelf_background_target_color(),
            ShelfConfig::Get()->GetDefaultShelfColor(
                GetPrimaryShelf()->shelf_widget()));
}

// Verify the target color of the shelf background is updated based on session
// state, starting from OOBE.
TEST_F(ShelfBackgroundTargetColorTest, ShelfBackgroundColorUpdatedFromOOBE) {
  ShelfBackgroundAnimatorTestApi test_api(
      Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
          ->shelf_widget()
          ->background_animator_for_testing());

  NotifySessionStateChanged(session_manager::SessionState::OOBE);
  EXPECT_EQ(test_api.shelf_background_target_color(), SK_ColorTRANSPARENT);

  SimulateUserLogin("user1@test.com");

  NotifySessionStateChanged(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_EQ(test_api.shelf_background_target_color(), SK_ColorTRANSPARENT);

  NotifySessionStateChanged(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(test_api.shelf_background_target_color(),
            ShelfConfig::Get()->GetDefaultShelfColor(
                GetPrimaryShelf()->shelf_widget()));
}

}  // namespace ash
