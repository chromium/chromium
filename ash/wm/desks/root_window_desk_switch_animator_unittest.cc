// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/root_window_desk_switch_animator.h"

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/root_window_desk_switch_animator_test_api.h"
#include "base/run_loop.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {

namespace {

// Computes the animation layer expected size. Each screenshot is the size of
// the primary root window, and there is spacing between each screenshot, which
// are children of the animation layer. The animation layer has padding on each
// end.
gfx::Size ComputeAnimationLayerExpectedSize(int expected_screenshots) {
  const gfx::Size root_window_size =
      Shell::GetPrimaryRootWindow()->bounds().size();
  const int edge_padding =
      std::round(kEdgePaddingRatio * root_window_size.width());
  gfx::Size expected_size = root_window_size;
  expected_size.set_width(root_window_size.width() * expected_screenshots +
                          kDesksSpacing * (expected_screenshots - 1) +
                          2 * edge_padding);
  return expected_size;
}

// Computes where |child_layer| is shown in root window coordinates. This is
// done by applying its parent's transform to it. |child_layer| itself is not
// expected to have a transform and its grandparent is the root window. If
// |use_target_transform| is false apply the parent's current transform,
// otherwise apply the parent's target transform (the expected transform at the
// end of an ongoing animation).
gfx::Rect GetVisibleBounds(ui::Layer* child_layer,
                           ui::Layer* animating_layer,
                           bool use_target_transform = false) {
  DCHECK_EQ(animating_layer, child_layer->parent());
  DCHECK(child_layer->transform().IsIdentity());
  DCHECK_EQ(Shell::GetPrimaryRootWindow()->layer(), animating_layer->parent());

  const gfx::Transform animating_layer_transform =
      use_target_transform ? animating_layer->GetTargetTransform()
                           : animating_layer->transform();
  DCHECK(animating_layer_transform.IsIdentityOr2dTranslation());
  return animating_layer_transform.MapRect(child_layer->bounds());
}

gfx::Rect GetTargetVisibleBounds(ui::Layer* child_layer,
                                 ui::Layer* animating_layer) {
  return GetVisibleBounds(child_layer, animating_layer,
                          /*use_target_transform=*/true);
}

}  // namespace

class RootWindowDeskSwitchAnimatorTest
    : public AshTestBase,
      public RootWindowDeskSwitchAnimator::Delegate {
 public:
  RootWindowDeskSwitchAnimatorTest() = default;
  RootWindowDeskSwitchAnimatorTest(const RootWindowDeskSwitchAnimatorTest&) =
      delete;
  RootWindowDeskSwitchAnimatorTest& operator=(
      const RootWindowDeskSwitchAnimatorTest&) = delete;
  ~RootWindowDeskSwitchAnimatorTest() override = default;

  void TearDown() override {
    // Shell teardown happens in AshTestBase::TearDown. That function is
    // executed before the destructor of derived tests. The animator is a
    // ShellObserver, so it wants the Shell to be around when it destroys. In
    // order for the Shell to be alive for the entire duration of the animator,
    // we explicitly destroy it here before proceeding with the rest of the
    // teardown.
    animator_.reset();

    AshTestBase::TearDown();
  }

  RootWindowDeskSwitchAnimatorTestApi* test_api() { return test_api_.get(); }
  RootWindowDeskSwitchAnimator* animator() { return animator_.get(); }

  int starting_desk_screenshot_taken_count() const {
    return starting_desk_screenshot_taken_count_;
  }
  int ending_desk_screenshot_taken_count() const {
    return ending_desk_screenshot_taken_count_;
  }
  int visible_desk_changed_count() const { return visible_desk_changed_count_; }

  // Creates an animator from the given indices on the primary root window.
  // Creates a test api for the animator as well.
  void InitAnimator(DeskSwitchAnimationType type,
                    int starting_desk_index,
                    int ending_desk_index) {
    animator_ = std::make_unique<RootWindowDeskSwitchAnimator>(
        Shell::GetPrimaryRootWindow(), type, starting_desk_index,
        ending_desk_index, this, /*for_remove=*/false);
    test_api_ =
        std::make_unique<RootWindowDeskSwitchAnimatorTestApi>(animator_.get());
  }

  // Wrappers for Take{Starting|Ending}DeskScreenshot that wait for the async
  // operation to finish.
  void TakeStartingDeskScreenshotAndWait() {
    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    animator_->TakeStartingDeskScreenshot();
    run_loop.Run();
  }

  void TakeEndingDeskScreenshotAndWait() {
    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    animator_->TakeEndingDeskScreenshot();
    run_loop.Run();
  }

  // RootWindowDeskSwitchAnimator::Delegate:
  void OnStartingDeskScreenshotTaken(int ending_desk_index) override {
    ++starting_desk_screenshot_taken_count_;

    DCHECK(!run_loop_quit_closure_.is_null());
    std::move(run_loop_quit_closure_).Run();
  }

  void OnEndingDeskScreenshotTaken() override {
    ++ending_desk_screenshot_taken_count_;

    DCHECK(!run_loop_quit_closure_.is_null());
    std::move(run_loop_quit_closure_).Run();
  }

  void OnDeskSwitchAnimationFinished() override {}

 private:
  // The RootWindowDeskSwitchAnimator we are testing.
  std::unique_ptr<RootWindowDeskSwitchAnimator> animator_;

  // The support test api associated with |animator_|.
  std::unique_ptr<RootWindowDeskSwitchAnimatorTestApi> test_api_;

  // Run loop quit closure for waiting for both starting and ending screenshots.
  base::OnceClosure run_loop_quit_closure_;

  int starting_desk_screenshot_taken_count_ = 0;
  int ending_desk_screenshot_taken_count_ = 0;
  int visible_desk_changed_count_ = 0;
};

// Tests a simple animation from one desk to another with quick animation.
TEST_F(RootWindowDeskSwitchAnimatorTest, SimpleQuickAnimation) {
  InitAnimator(DeskSwitchAnimationType::kQuickAnimation, 1, 2);
  TakeStartingDeskScreenshotAndWait();
  TakeEndingDeskScreenshotAndWait();

  // Tests that a simple animation has 2 screenshots, one for each desk.
  EXPECT_EQ(1, starting_desk_screenshot_taken_count());
  EXPECT_EQ(1, ending_desk_screenshot_taken_count());

  // Tests that the animation layer is the expected size.
  auto* animation_layer = test_api()->GetAnimationLayer();
  EXPECT_EQ(2u, animation_layer->children().size());
  // With quick animation, the screenshot lays over each other with a 25%
  // offset.
  const gfx::Size root_window_size =
      Shell::GetPrimaryRootWindow()->bounds().size();
  const int edge_padding =
      std::round(kEdgePaddingRatio * root_window_size.width());
  gfx::Size expected_size = root_window_size;
  expected_size.set_width(root_window_size.width() * 1.25 + 2 * edge_padding);
  EXPECT_EQ(expected_size, animation_layer->bounds().size());

  // Tests that the screenshot associated with desk index 1 is the one that is
  // shown at the beginning of the animation.
  EXPECT_EQ(Shell::GetPrimaryRootWindow()->bounds(),
            GetVisibleBounds(test_api()->GetScreenshotLayerOfDeskWithIndex(1),
                             animation_layer));

  // Tests that the screenshot associated with desk index 2 has 0 opacity at the
  // beginning of the animation.
  EXPECT_EQ(test_api()->GetScreenshotLayerOfDeskWithIndex(2)->opacity(), 0);

  // Tests that the screenshot associated with desk index 2 is the one that is
  // shown at the end of the animation.
  animator()->StartAnimation();
  EXPECT_EQ(Shell::GetPrimaryRootWindow()->bounds(),
            GetVisibleBounds(test_api()->GetScreenshotLayerOfDeskWithIndex(2),
                             animation_layer));
  EXPECT_EQ(2, test_api()->GetEndingDeskIndex());
  // Tests that the screenshot associated with desk index 2 has 1 opacity at the
  // ending of the animation.
  EXPECT_EQ(test_api()->GetScreenshotLayerOfDeskWithIndex(2)->opacity(), 1);
}

// Tests a quick animation with interrupt will not cause crash.
TEST_F(RootWindowDeskSwitchAnimatorTest, InterruptQuickAnimation) {
  InitAnimator(DeskSwitchAnimationType::kQuickAnimation, 1, 2);
  TakeStartingDeskScreenshotAndWait();
  TakeEndingDeskScreenshotAndWait();

  // Replacing needs to be done while a current animation is underway, otherwise
  // it will have no effect.
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  animator()->StartAnimation();
  // Replacing with an animation going back to desk index 1. No new screenshot
  // is needed.
  bool needs_screenshot = animator()->ReplaceAnimation(1);
  EXPECT_FALSE(needs_screenshot);

  // Tests that no new screenshot was taken as it already existed.
  auto* animation_layer = test_api()->GetAnimationLayer();
  EXPECT_EQ(2u, animation_layer->children().size());
  EXPECT_EQ(1, starting_desk_screenshot_taken_count());
  EXPECT_EQ(1, ending_desk_screenshot_taken_count());

  // Tests that the screenshot associated with desk index 1 has 0 opacity at the
  // beginning of the animation.
  EXPECT_EQ(test_api()->GetScreenshotLayerOfDeskWithIndex(1)->opacity(), 0);

  // Tests that the screenshot associated with desk index 1 is the one that is
  // shown at the end of the animation.
  animator()->StartAnimation();

  EXPECT_EQ(
      Shell::GetPrimaryRootWindow()->bounds(),
      GetTargetVisibleBounds(test_api()->GetScreenshotLayerOfDeskWithIndex(1),
                             animation_layer));
}

// Tests a simple animation from one desk to another with continuous animation.
TEST_F(RootWindowDeskSwitchAnimatorTest, SimpleContinuousAnimation) {
  InitAnimator(DeskSwitchAnimationType::kContinuousAnimation, 1, 2);
  TakeStartingDeskScreenshotAndWait();
  TakeEndingDeskScreenshotAndWait();

  // Tests that a simple animation has 2 screenshots, one for each desk.
  EXPECT_EQ(1, starting_desk_screenshot_taken_count());
  EXPECT_EQ(1, ending_desk_screenshot_taken_count());

  // Tests that the animation layer is the expected size.
  auto* animation_layer = test_api()->GetAnimationLayer();
  EXPECT_EQ(2u, animation_layer->children().size());
  EXPECT_EQ(ComputeAnimationLayerExpectedSize(2),
            animation_layer->bounds().size());

  // Tests that the screenshot associated with desk index 1 is the one that is
  // shown at the beginning of the animation.
  EXPECT_EQ(Shell::GetPrimaryRootWindow()->bounds(),
            GetVisibleBounds(test_api()->GetScreenshotLayerOfDeskWithIndex(1),
                             animation_layer));

  // Tests that the screenshot associated with desk index 2 is the one that is
  // shown at the end of the animation.
  animator()->StartAnimation();
  EXPECT_EQ(Shell::GetPrimaryRootWindow()->bounds(),
            GetVisibleBounds(test_api()->GetScreenshotLayerOfDeskWithIndex(2),
                             animation_layer));
  EXPECT_EQ(2, test_api()->GetEndingDeskIndex());
}

// TODO(b/219068687): Re-enable chained desk animation tests.
// Tests a chained animation where the replaced animation already has a
// screenshot layer stored.
TEST_F(RootWindowDeskSwitchAnimatorTest,
       DISABLED_ChainedAnimationNoNewScreenshot) {
  InitAnimator(DeskSwitchAnimationType::kContinuousAnimation, 1, 2);
  TakeStartingDeskScreenshotAndWait();
  TakeEndingDeskScreenshotAndWait();

  // Replacing needs to be done while a current animation is underway, otherwise
  // it will have no effect.
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  animator()->StartAnimation();
  // Replacing with an animation going back to desk index 1. No new screenshot
  // is needed.
  bool needs_screenshot = animator()->ReplaceAnimation(1);
  EXPECT_FALSE(needs_screenshot);

  // Tests that no new screenshot was taken as it already existed.
  auto* animation_layer = test_api()->GetAnimationLayer();
  EXPECT_EQ(2u, animation_layer->children().size());
  EXPECT_EQ(1, starting_desk_screenshot_taken_count());
  EXPECT_EQ(1, ending_desk_screenshot_taken_count());

  // Tests that the screenshot associated with desk index 1 is the one that is
  // shown at the end of the animation.
  animator()->StartAnimation();
  EXPECT_EQ(
      Shell::GetPrimaryRootWindow()->bounds(),
      GetTargetVisibleBounds(test_api()->GetScreenshotLayerOfDeskWithIndex(1),
                             animation_layer));
}

// TODO(b/219068687): Re-enable chained desk animation tests.
// Tests a chained animation where we are adding an animation to the right of
// the current animating desks, causing the animation layer to shift left.
TEST_F(RootWindowDeskSwitchAnimatorTest, DISABLED_ChainedAnimationMovingLeft) {
  InitAnimator(DeskSwitchAnimationType::kContinuousAnimation, 1, 2);
  TakeStartingDeskScreenshotAndWait();
  TakeEndingDeskScreenshotAndWait();

  // Replacing needs to be done while a current animation is underway, otherwise
  // it will have no effect.
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Tests that the animation layer originally has 2 children.
  auto* animation_layer = test_api()->GetAnimationLayer();
  EXPECT_EQ(2u, animation_layer->children().size());

  animator()->StartAnimation();

  // Replace the current animation to one that goes to desk index 3.
  EXPECT_EQ(2, test_api()->GetEndingDeskIndex());
  bool needs_screenshot = animator()->ReplaceAnimation(3);
  ASSERT_TRUE(needs_screenshot);
  EXPECT_EQ(3, test_api()->GetEndingDeskIndex());

  // Take a screenshot at the new ending desk. Test that the animation layer now
  // has 3 children.
  TakeEndingDeskScreenshotAndWait();
  EXPECT_EQ(1, starting_desk_screenshot_taken_count());
  EXPECT_EQ(2, ending_desk_screenshot_taken_count());
  EXPECT_EQ(3u, animation_layer->children().size());
  EXPECT_EQ(ComputeAnimationLayerExpectedSize(3),
            animation_layer->bounds().size());

  animator()->StartAnimation();
  // Tests that the screenshot associated with desk index 3 is the one that is
  // shown at the end of the animation.
  EXPECT_EQ(
      Shell::GetPrimaryRootWindow()->bounds(),
      GetTargetVisibleBounds(test_api()->GetScreenshotLayerOfDeskWithIndex(3),
                             animation_layer));
}

// TODO(b/219068687): Re-enable chained desk animation tests.
// Tests a chained animation where we are adding an animation to the left of
// the current animating desks, causing the animation layer to shift right.
TEST_F(RootWindowDeskSwitchAnimatorTest, DISABLED_ChainedAnimationMovingRight) {
  InitAnimator(DeskSwitchAnimationType::kContinuousAnimation, 3, 2);
  TakeStartingDeskScreenshotAndWait();
  TakeEndingDeskScreenshotAndWait();

  // Replacing needs to be done while a current animation is underway, otherwise
  // it will have no effect.
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  animator()->StartAnimation();

  // Replace the current animation to one that goes to desk index 1.
  EXPECT_EQ(2, test_api()->GetEndingDeskIndex());
  bool needs_screenshot = animator()->ReplaceAnimation(1);
  ASSERT_TRUE(needs_screenshot);
  EXPECT_EQ(1, test_api()->GetEndingDeskIndex());

  // Take a screenshot at the new ending desk. Test that the animation layer now
  // has 3 children.
  TakeEndingDeskScreenshotAndWait();
  EXPECT_EQ(1, starting_desk_screenshot_taken_count());
  EXPECT_EQ(2, ending_desk_screenshot_taken_count());
  auto* animation_layer = test_api()->GetAnimationLayer();
  EXPECT_EQ(3u, animation_layer->children().size());
  EXPECT_EQ(ComputeAnimationLayerExpectedSize(3),
            animation_layer->bounds().size());

  animator()->StartAnimation();
  // Tests that the screenshot associated with desk index 1 is the one that is
  // shown at the end of the animation.
  EXPECT_EQ(
      Shell::GetPrimaryRootWindow()->bounds(),
      GetTargetVisibleBounds(test_api()->GetScreenshotLayerOfDeskWithIndex(1),
                             animation_layer));
}

// TODO(b/219068687): Re-enable chained desk animation tests.
// Tests a complex animation which multiple animations are started and replaced.
TEST_F(RootWindowDeskSwitchAnimatorTest, DISABLED_MultipleReplacements) {
  InitAnimator(DeskSwitchAnimationType::kContinuousAnimation, 1, 2);
  TakeStartingDeskScreenshotAndWait();
  TakeEndingDeskScreenshotAndWait();

  // Replacing needs to be done while a current animation is underway, otherwise
  // it will have no effect.
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  animator()->StartAnimation();

  // Replace all the indices in the list one at a time.
  auto animation_indices = {1, 0, 1, 2, 1, 2, 3, 2, 1};
  ui::Layer* animation_layer = test_api()->GetAnimationLayer();
  for (int index : animation_indices) {
    if (animator()->ReplaceAnimation(index))
      TakeEndingDeskScreenshotAndWait();
    EXPECT_EQ(index, test_api()->GetEndingDeskIndex());

    // Start the replacement animation. The new animation should have a target
    // transform such that the desk at |index| is visible on animation end.
    animator()->StartAnimation();
    EXPECT_EQ(Shell::GetPrimaryRootWindow()->bounds(),
              GetTargetVisibleBounds(
                  test_api()->GetScreenshotLayerOfDeskWithIndex(index),
                  animation_layer));
  }

  // Only 4 screenshots are taken as they are reused.
  EXPECT_EQ(1, starting_desk_screenshot_taken_count());
  EXPECT_EQ(3, ending_desk_screenshot_taken_count());

  // Tests that the screenshot associated with desk index 1 is the one that is
  // shown at the end of the animation.
  EXPECT_EQ(
      Shell::GetPrimaryRootWindow()->bounds(),
      GetTargetVisibleBounds(test_api()->GetScreenshotLayerOfDeskWithIndex(1),
                             animation_layer));
}

// Tests that the update swipe animation api requests a new screenshot when
// needed.
TEST_F(RootWindowDeskSwitchAnimatorTest, UpdateSwipeAnimationNewScreenshot) {
  // Add two more desks as we need three desks for this test.
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);

  InitAnimator(DeskSwitchAnimationType::kContinuousAnimation, 0, 1);
  TakeStartingDeskScreenshotAndWait();
  TakeEndingDeskScreenshotAndWait();

  // Swipe so that half of desk indexed 0 and half of desk indexed 1 is shown.
  // Verify that a new screenshot is not needed, as the screenshots of both desk
  // 0 and desk 1 were taken when initializing.
  EXPECT_FALSE(
      animator()->UpdateSwipeAnimation(-kTouchpadSwipeLengthForDeskChange / 2));

  // Swipe so that desk indexed 1 is fully shown. Verify that a new screenshot
  // is needed as we expect the user to continue swiping to show desk indexed 2.
  EXPECT_TRUE(
      animator()->UpdateSwipeAnimation(-kTouchpadSwipeLengthForDeskChange / 2));
}

// Tests that additional swipes do not shift the animation layer if it has
// reached its limit.
TEST_F(RootWindowDeskSwitchAnimatorTest, UpdateSwipeAnimationLimit) {
  // Add one more desk as we need two desks for this test.
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);

  InitAnimator(DeskSwitchAnimationType::kContinuousAnimation, 0, 1);
  TakeStartingDeskScreenshotAndWait();
  TakeEndingDeskScreenshotAndWait();

  // Do a large right swipe; this will ensure we reach the limit on the left of
  // the animation layer.
  animator()->UpdateSwipeAnimation(5 * kTouchpadSwipeLengthForDeskChange);
  auto* animation_layer = test_api()->GetAnimationLayer();
  int x_translation = animation_layer->transform().To2dTranslation().x();

  // Test that an additional small right swipe will not shift the animation
  // layer.
  animator()->UpdateSwipeAnimation(5);
  EXPECT_EQ(x_translation, animation_layer->transform().To2dTranslation().x());

  // Swipe back to desk indexed 1.
  animator()->UpdateSwipeAnimation(-2 * kTouchpadSwipeLengthForDeskChange);

  // Do a large right swipe; this will ensure we reach the limit on the left of
  // the animation layer.
  animator()->UpdateSwipeAnimation(-5 * kTouchpadSwipeLengthForDeskChange);
  x_translation = animation_layer->transform().To2dTranslation().x();

  // Test that an additional small left swipe will not shift the animation
  // layer.
  animator()->UpdateSwipeAnimation(-5);
  EXPECT_EQ(x_translation, animation_layer->transform().To2dTranslation().x());
}

// Tests the when ending the swipe animation, we animate to the expected desk.
TEST_F(RootWindowDeskSwitchAnimatorTest, EndSwipeAnimation) {
  // Add one more desk as we need two desks for this test.
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);

  InitAnimator(DeskSwitchAnimationType::kContinuousAnimation, 0, 1);
  TakeStartingDeskScreenshotAndWait();
  TakeEndingDeskScreenshotAndWait();
  auto* animation_layer = test_api()->GetAnimationLayer();

  // Make a small left swipe headed towards desk indexed 1. Desk indexed 0
  // should still be the most visible desk, so on ending the swipe animation,
  // desk indexed 0 is the target desk.
  animator()->UpdateSwipeAnimation(-kTouchpadSwipeLengthForDeskChange / 10);
  animator()->EndSwipeAnimation(/*is_fast_swipe=*/false);
  EXPECT_EQ(
      Shell::GetPrimaryRootWindow()->bounds(),
      GetTargetVisibleBounds(test_api()->GetScreenshotLayerOfDeskWithIndex(0),
                             animation_layer));

  // Reinitialize the animator as each animator only supports one
  // EndSwipeAnimation during its lifetime.
  InitAnimator(DeskSwitchAnimationType::kContinuousAnimation, 0, 1);
  TakeStartingDeskScreenshotAndWait();
  TakeEndingDeskScreenshotAndWait();
  animation_layer = test_api()->GetAnimationLayer();

  // Make a big left swipe headed towards desk indexed 1. Desk indexed 1 should
  // be the new most visible desk, so on ending the swipe animation, desk
  // indexed 1 is the target desk.
  animator()->UpdateSwipeAnimation(-9 * kTouchpadSwipeLengthForDeskChange / 10);
  animator()->EndSwipeAnimation(/*is_fast_swipe=*/false);
  EXPECT_EQ(
      Shell::GetPrimaryRootWindow()->bounds(),
      GetTargetVisibleBounds(test_api()->GetScreenshotLayerOfDeskWithIndex(1),
                             animation_layer));
}

// Tests that a fast swipe, even if it is small will result in switching desks.
TEST_F(RootWindowDeskSwitchAnimatorTest, FastSwipe) {
  // Add one more desk as we need two desks for this test.
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);

  InitAnimator(DeskSwitchAnimationType::kContinuousAnimation, 0, 1);
  TakeStartingDeskScreenshotAndWait();
  TakeEndingDeskScreenshotAndWait();
  auto* animation_layer = test_api()->GetAnimationLayer();

  // Make a small left swipe headed towards desk indexed 1. We should still
  // animate to desk indexed 1 even though desk indexed 0 is the most visible
  // desk since it is a fast swipe.
  animator()->UpdateSwipeAnimation(-kTouchpadSwipeLengthForDeskChange / 5);
  animator()->EndSwipeAnimation(/*is_fast_swipe=*/true);
  EXPECT_EQ(
      Shell::GetPrimaryRootWindow()->bounds(),
      GetTargetVisibleBounds(test_api()->GetScreenshotLayerOfDeskWithIndex(1),
                             animation_layer));
}

// Test that there is no crash if we end swiping before the desk animation
// screenshots are finished taking. Regression test for
// https://crbug.com/1134390.
TEST_F(RootWindowDeskSwitchAnimatorTest,
       EndSwipeAnimationBeforeScreenshotTaken) {
  InitAnimator(DeskSwitchAnimationType::kContinuousAnimation, 0, 1);
  animator()->TakeStartingDeskScreenshot();
  animator()->EndSwipeAnimation(/*is_fast_swipe=*/false);

  // Reinitialize the animator as each animator only supports one
  // EndSwipeAnimation during its lifetime.
  InitAnimator(DeskSwitchAnimationType::kContinuousAnimation, 0, 1);
  TakeStartingDeskScreenshotAndWait();
  animator()->TakeEndingDeskScreenshot();
  animator()->EndSwipeAnimation(/*is_fast_swipe=*/false);
}

}  // namespace ash
