// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_background_animator.h"

#include <memory>

#include "ash/animation/animation_change_type.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/color_palette.h"

namespace ash {
namespace {

static auto kMaxAlpha = ShelfBackgroundAnimator::kMaxAlpha;

// A valid color value that is distinct from any final animation state values.
// Used to check if color values are changed during animations.
const SkColor kDummyColor = SK_ColorBLUE;

// Helper function to get the base color from |color|, i.e., remove the alpha.
SkColor GetBaseColor(SkColor color) {
  return SkColorSetRGB(SkColorGetR(color), SkColorGetG(color),
                       SkColorGetB(color));
}

// Observer that caches color values for the last observation.
class TestShelfBackgroundObserver : public ShelfBackgroundAnimatorObserver {
 public:
  TestShelfBackgroundObserver() = default;
  ~TestShelfBackgroundObserver() override = default;

  SkColor background_color() const { return background_color_; }

  // Convenience function to get the alpha value from |background_color_|.
  int GetBackgroundAlpha() const;

  SkColor item_background_color() const { return item_background_color_; }

  // Convenience function to get the alpha value from |item_background_color_|.
  int GetItemBackgroundAlpha() const;

  // ShelfBackgroundObserver:
  void UpdateShelfBackground(SkColor color) override;
  void UpdateShelfItemBackground(SkColor color) override;

 private:
  int background_color_ = SK_ColorTRANSPARENT;
  int item_background_color_ = SK_ColorTRANSPARENT;

  DISALLOW_COPY_AND_ASSIGN(TestShelfBackgroundObserver);
};

int TestShelfBackgroundObserver::GetBackgroundAlpha() const {
  return SkColorGetA(background_color_);
}

int TestShelfBackgroundObserver::GetItemBackgroundAlpha() const {
  return SkColorGetA(item_background_color_);
}

void TestShelfBackgroundObserver::UpdateShelfBackground(SkColor color) {
  background_color_ = color;
}

void TestShelfBackgroundObserver::UpdateShelfItemBackground(SkColor color) {
  item_background_color_ = color;
}

}  // namespace

// Provides internal access to a ShelfBackgroundAnimator instance.
class ShelfBackgroundAnimatorTestApi {
 public:
  explicit ShelfBackgroundAnimatorTestApi(ShelfBackgroundAnimator* animator)
      : animator_(animator) {}

  ~ShelfBackgroundAnimatorTestApi() = default;

  ShelfBackgroundType previous_background_type() const {
    return animator_->previous_background_type_;
  }

  gfx::SlideAnimation* animator() const { return animator_->animator_.get(); }

  SkColor shelf_background_target_color() const {
    return animator_->shelf_background_values_.target_color();
  }

  SkColor item_background_target_color() const {
    return animator_->item_background_values_.target_color();
  }

 private:
  // The instance to provide internal access to.
  ShelfBackgroundAnimator* animator_;

  DISALLOW_COPY_AND_ASSIGN(ShelfBackgroundAnimatorTestApi);
};

class ShelfBackgroundAnimatorTest : public testing::Test {
 public:
  ShelfBackgroundAnimatorTest() = default;
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

  // Completes all the animations.
  void CompleteAnimations();

  TestShelfBackgroundObserver observer_;

  // Test target.
  std::unique_ptr<ShelfBackgroundAnimator> animator_;

  // Provides internal access to |animator_|.
  std::unique_ptr<ShelfBackgroundAnimatorTestApi> test_api_;

  // Used to control the animations.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

 private:
  std::unique_ptr<base::ThreadTaskRunnerHandle> task_runner_handle_;

  DISALLOW_COPY_AND_ASSIGN(ShelfBackgroundAnimatorTest);
};

void ShelfBackgroundAnimatorTest::SetUp() {
  task_runner_ = new base::TestMockTimeTaskRunner();
  task_runner_handle_.reset(new base::ThreadTaskRunnerHandle(task_runner_));

  animator_.reset(
      new ShelfBackgroundAnimator(SHELF_BACKGROUND_DEFAULT, nullptr, nullptr));
  animator_->AddObserver(&observer_);

  test_api_.reset(new ShelfBackgroundAnimatorTestApi(animator_.get()));
}

void ShelfBackgroundAnimatorTest::PaintBackground(
    ShelfBackgroundType background_type,
    AnimationChangeType change_type) {
  animator_->PaintBackground(background_type, change_type);
}

void ShelfBackgroundAnimatorTest::SetColorValuesOnObserver(SkColor color) {
  observer_.UpdateShelfBackground(color);
  observer_.UpdateShelfItemBackground(color);
}

void ShelfBackgroundAnimatorTest::CompleteAnimations() {
  task_runner_->FastForwardUntilNoTasksRemain();
}

// Verify the |previous_background_type_| and |target_background_type_| values
// when animating to the same target type multiple times.
TEST_F(ShelfBackgroundAnimatorTest, BackgroundTypesWhenAnimatingToSameTarget) {
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, animator_->target_background_type());

  PaintBackground(SHELF_BACKGROUND_DEFAULT);
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, animator_->target_background_type());
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, test_api_->previous_background_type());

  PaintBackground(SHELF_BACKGROUND_DEFAULT);
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, animator_->target_background_type());
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, test_api_->previous_background_type());
}

// Verify subsequent calls to PaintBackground() using the
// AnimationChangeType::ANIMATE change type are ignored.
TEST_F(ShelfBackgroundAnimatorTest,
       MultipleAnimateCallsToSameTargetAreIgnored) {
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED);
  SetColorValuesOnObserver(kDummyColor);
  animator_->PaintBackground(SHELF_BACKGROUND_DEFAULT,
                             AnimationChangeType::ANIMATE);
  CompleteAnimations();

  EXPECT_NE(observer_.background_color(), kDummyColor);
  EXPECT_NE(observer_.item_background_color(), kDummyColor);

  SetColorValuesOnObserver(kDummyColor);
  animator_->PaintBackground(SHELF_BACKGROUND_DEFAULT,
                             AnimationChangeType::ANIMATE);
  CompleteAnimations();

  EXPECT_EQ(observer_.background_color(), kDummyColor);
  EXPECT_EQ(observer_.item_background_color(), kDummyColor);
}

// Verify observers are updated with the current values when they are added.
TEST_F(ShelfBackgroundAnimatorTest, ObserversUpdatedWhenAdded) {
  animator_->RemoveObserver(&observer_);
  SetColorValuesOnObserver(kDummyColor);

  animator_->AddObserver(&observer_);

  EXPECT_NE(observer_.background_color(), kDummyColor);
  EXPECT_NE(observer_.item_background_color(), kDummyColor);
}

// Verify the alpha values for the SHELF_BACKGROUND_DEFAULT state.
TEST_F(ShelfBackgroundAnimatorTest, DefaultBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);

  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, animator_->target_background_type());
  EXPECT_EQ(kShelfTranslucentAlpha, observer_.GetBackgroundAlpha());
  EXPECT_EQ(0, observer_.GetItemBackgroundAlpha());
}

// Verify the alpha values for the SHELF_BACKGROUND_OVERLAP state.
TEST_F(ShelfBackgroundAnimatorTest, OverlapBackground) {
  PaintBackground(SHELF_BACKGROUND_OVERLAP);

  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, animator_->target_background_type());
  EXPECT_EQ(kShelfTranslucentAlpha, observer_.GetBackgroundAlpha());
  EXPECT_EQ(0, observer_.GetItemBackgroundAlpha());
}

// Verify the alpha values for the SHELF_BACKGROUND_MAXIMIZED state.
TEST_F(ShelfBackgroundAnimatorTest, MaximizedBackground) {
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED);

  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, animator_->target_background_type());
  EXPECT_EQ(kShelfTranslucentMaximizedWindow, observer_.GetBackgroundAlpha());
  EXPECT_EQ(0, observer_.GetItemBackgroundAlpha());
}

// Verify the alpha values for the SHELF_BACKGROUND_SPLIT_VIEW state.
TEST_F(ShelfBackgroundAnimatorTest, SplitViewBackground) {
  PaintBackground(SHELF_BACKGROUND_SPLIT_VIEW);

  EXPECT_EQ(SHELF_BACKGROUND_SPLIT_VIEW, animator_->target_background_type());
  EXPECT_EQ(kMaxAlpha, observer_.GetBackgroundAlpha());
  EXPECT_EQ(0, observer_.GetItemBackgroundAlpha());
}

// Crashes on ChromeOS .  http://crbug.com/878944
#if defined(OS_CHROMEOS)
#define MAYBE_FullscreenAppListBackground DISABLED_FullscreenAppListBackground
#else
#define MAYBE_FullscreenAppListBackground FullscreenAppListBackground
#endif

// Verify the alpha values for the SHELF_BACKGROUND_APP_LIST state.
TEST_F(ShelfBackgroundAnimatorTest, MAYBE_FullscreenAppListBackground) {
  PaintBackground(SHELF_BACKGROUND_APP_LIST);

  EXPECT_EQ(SHELF_BACKGROUND_APP_LIST, animator_->target_background_type());
  EXPECT_EQ(kShelfTranslucentOverAppList, observer_.GetBackgroundAlpha());
  EXPECT_EQ(0, observer_.GetItemBackgroundAlpha());
}

TEST_F(ShelfBackgroundAnimatorTest,
       AnimatorIsDetroyedWhenCompletingSuccessfully) {
  PaintBackground(SHELF_BACKGROUND_OVERLAP, AnimationChangeType::ANIMATE);
  EXPECT_TRUE(test_api_->animator());
  CompleteAnimations();
  EXPECT_FALSE(test_api_->animator());
}

TEST_F(ShelfBackgroundAnimatorTest,
       AnimatorDestroyedWhenChangingBackgroundImmediately) {
  PaintBackground(SHELF_BACKGROUND_OVERLAP, AnimationChangeType::ANIMATE);
  EXPECT_TRUE(test_api_->animator());

  PaintBackground(SHELF_BACKGROUND_OVERLAP, AnimationChangeType::IMMEDIATE);
  EXPECT_FALSE(test_api_->animator());
}

// Verify that existing animator is used when animating to the previous state.
TEST_F(ShelfBackgroundAnimatorTest,
       ExistingAnimatorIsReusedWhenAnimatingToPreviousState) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT, AnimationChangeType::ANIMATE);
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED, AnimationChangeType::ANIMATE);

  const gfx::SlideAnimation* animator = test_api_->animator();
  EXPECT_TRUE(animator);

  PaintBackground(SHELF_BACKGROUND_DEFAULT, AnimationChangeType::ANIMATE);

  EXPECT_EQ(animator, test_api_->animator());
}

// Verify that existing animator is not re-used when the target background isn't
// the same as the previous background.
TEST_F(ShelfBackgroundAnimatorTest,
       ExistingAnimatorNotReusedWhenTargetBackgroundNotPreviousBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT, AnimationChangeType::ANIMATE);
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED, AnimationChangeType::ANIMATE);

  const gfx::SlideAnimation* animator = test_api_->animator();
  EXPECT_TRUE(animator);

  EXPECT_NE(SHELF_BACKGROUND_OVERLAP, test_api_->previous_background_type());
  PaintBackground(SHELF_BACKGROUND_OVERLAP, AnimationChangeType::ANIMATE);

  EXPECT_NE(animator, test_api_->animator());
}

// Verify observers are always notified, even when alpha values don't change.
TEST_F(ShelfBackgroundAnimatorTest,
       ObserversAreNotifiedWhenSnappingToSameTargetBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);
  SetColorValuesOnObserver(kDummyColor);
  PaintBackground(SHELF_BACKGROUND_DEFAULT);

  EXPECT_NE(observer_.background_color(), kDummyColor);
  EXPECT_NE(observer_.item_background_color(), kDummyColor);
}

class ShelfBackgroundTargetColorTest : public NoSessionAshTestBase {
 public:
  ShelfBackgroundTargetColorTest() = default;
  ~ShelfBackgroundTargetColorTest() override = default;

  // AshTestBase:
  void SetUp() override {
    // Do not allow the shelf color to be derived from the wallpaper, in order
    // to have a fixed color in tests.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAshShelfColor, switches::kAshShelfColorDisabled);
    AshTestBase::SetUp();
  }

 protected:
  // Helper function to notify session state changes.
  void NotifySessionStateChanged(session_manager::SessionState state) {
    GetSessionControllerClient()->SetSessionState(state);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfBackgroundTargetColorTest);
};

// The tests only compare the base color, because different alpha values may be
// applied based on |ShelfBackgroundType|, which is verifed by
// |ShelfBackgroundAnimatorTest|.
//
// Verify the target colors of the shelf and item backgrounds are updated based
// on session state, starting from LOGIN_PRIMARY.
TEST_F(ShelfBackgroundTargetColorTest,
       ShelfAndItemBackgroundColorUpdatedFromLogin) {
  ShelfBackgroundAnimatorTestApi test_api(
      Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
          ->shelf_widget()
          ->background_animator_for_testing());

  NotifySessionStateChanged(session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(GetBaseColor(test_api.shelf_background_target_color()),
            GetBaseColor(SK_ColorTRANSPARENT));
  EXPECT_EQ(GetBaseColor(test_api.item_background_target_color()),
            GetBaseColor(SK_ColorTRANSPARENT));

  SimulateUserLogin("user1@test.com");

  NotifySessionStateChanged(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_EQ(GetBaseColor(test_api.shelf_background_target_color()),
            GetBaseColor(SK_ColorTRANSPARENT));
  EXPECT_EQ(GetBaseColor(test_api.item_background_target_color()),
            GetBaseColor(SK_ColorTRANSPARENT));

  // The shelf has a non-transparent background only when session state is
  // active.
  NotifySessionStateChanged(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(GetBaseColor(test_api.shelf_background_target_color()),
            GetBaseColor(kShelfDefaultBaseColor));
  EXPECT_EQ(GetBaseColor(test_api.item_background_target_color()),
            GetBaseColor(kShelfDefaultBaseColor));

  NotifySessionStateChanged(session_manager::SessionState::LOCKED);
  EXPECT_EQ(GetBaseColor(test_api.shelf_background_target_color()),
            GetBaseColor(SK_ColorTRANSPARENT));
  EXPECT_EQ(GetBaseColor(test_api.item_background_target_color()),
            GetBaseColor(SK_ColorTRANSPARENT));

  // Ensure the shelf background color is correct after unlocking.
  NotifySessionStateChanged(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(GetBaseColor(test_api.shelf_background_target_color()),
            GetBaseColor(kShelfDefaultBaseColor));
  EXPECT_EQ(GetBaseColor(test_api.item_background_target_color()),
            GetBaseColor(kShelfDefaultBaseColor));

  NotifySessionStateChanged(session_manager::SessionState::LOGIN_SECONDARY);
  EXPECT_EQ(GetBaseColor(test_api.shelf_background_target_color()),
            GetBaseColor(SK_ColorTRANSPARENT));
  EXPECT_EQ(GetBaseColor(test_api.item_background_target_color()),
            GetBaseColor(SK_ColorTRANSPARENT));

  // Ensure the shelf background color is correct after closing the user adding
  // screen.
  NotifySessionStateChanged(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(GetBaseColor(test_api.shelf_background_target_color()),
            GetBaseColor(kShelfDefaultBaseColor));
  EXPECT_EQ(GetBaseColor(test_api.item_background_target_color()),
            GetBaseColor(kShelfDefaultBaseColor));
}

// Verify the target colors of the shelf and item backgrounds are updated based
// on session state, starting from OOBE.
// Note: the shelf is not supported for OOBE yet but it's good to check it here.
// TODO(wzang|798869): The item backgrounds still keep the OOBE color if
// directly transitioned from OOBE to LOGIN_PRIMARY. Revisit this when OOBE
// shelf is supported.
TEST_F(ShelfBackgroundTargetColorTest,
       ShelfAndItemBackgroundColorUpdatedFromOOBE) {
  ShelfBackgroundAnimatorTestApi test_api(
      Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
          ->shelf_widget()
          ->background_animator_for_testing());

  NotifySessionStateChanged(session_manager::SessionState::OOBE);
  EXPECT_EQ(GetBaseColor(test_api.shelf_background_target_color()),
            GetBaseColor(SK_ColorTRANSPARENT));
  EXPECT_EQ(GetBaseColor(test_api.item_background_target_color()),
            GetBaseColor(gfx::kGoogleGrey100));

  SimulateUserLogin("user1@test.com");

  NotifySessionStateChanged(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_EQ(GetBaseColor(test_api.shelf_background_target_color()),
            GetBaseColor(SK_ColorTRANSPARENT));
  EXPECT_EQ(GetBaseColor(test_api.item_background_target_color()),
            GetBaseColor(SK_ColorTRANSPARENT));

  NotifySessionStateChanged(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(GetBaseColor(test_api.shelf_background_target_color()),
            GetBaseColor(kShelfDefaultBaseColor));
  EXPECT_EQ(GetBaseColor(test_api.item_background_target_color()),
            GetBaseColor(kShelfDefaultBaseColor));
}

}  // namespace ash
