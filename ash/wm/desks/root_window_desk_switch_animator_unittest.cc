// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/root_window_desk_switch_animator.h"

#include <memory>

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/root_window_desk_switch_animator_test_api.h"
#include "base/test/scoped_feature_list.h"
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
      std::round(RootWindowDeskSwitchAnimator::kEdgePaddingRatio *
                 root_window_size.width());
  gfx::Size expected_size = root_window_size;
  expected_size.set_width(root_window_size.width() * expected_screenshots +
                          RootWindowDeskSwitchAnimator::kDesksSpacing *
                              (expected_screenshots - 1) +
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
  DCHECK(animating_layer_transform.IsIdentityOr2DTranslation());
  gfx::RectF bounds(child_layer->bounds());
  animating_layer_transform.TransformRect(&bounds);
  return gfx::ToRoundedRect(bounds);
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

  RootWindowDeskSwitchAnimatorTestApi* test_api() { return test_api_.get(); }
  RootWindowDeskSwitchAnimator* animator() { return animator_.get(); }

  int starting_desk_screenshot_taken_count() const {
    return starting_desk_screenshot_taken_count_;
  }
  int ending_desk_screenshot_taken_count() const {
    return ending_desk_screenshot_taken_count_;
  }

  // Creates an animator from the given indices on the primary root window.
  // Creates a test api for the animator as well.
  void InitAnimator(int starting_desk_index, int ending_desk_index) {
    animator_ = std::make_unique<RootWindowDeskSwitchAnimator>(
        Shell::GetPrimaryRootWindow(), starting_desk_index, ending_desk_index,
        this, /*for_remove=*/false);
    test_api_ =
        std::make_unique<RootWindowDeskSwitchAnimatorTestApi>(animator_.get());
  }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnhancedDeskAnimations);
    AshTestBase::SetUp();
  }

  // RootWindowDeskSwitchAnimator::Delegate:
  void OnStartingDeskScreenshotTaken(int ending_desk_index) override {
    ++starting_desk_screenshot_taken_count_;
  }

  void OnEndingDeskScreenshotTaken() override {
    ++ending_desk_screenshot_taken_count_;
  }

  void OnDeskSwitchAnimationFinished() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // The RootWindowDeskSwitchAnimator we are testing.
  std::unique_ptr<RootWindowDeskSwitchAnimator> animator_;

  // The support test api associated with |animator_|.
  std::unique_ptr<RootWindowDeskSwitchAnimatorTestApi> test_api_;

  int starting_desk_screenshot_taken_count_ = 0;
  int ending_desk_screenshot_taken_count_ = 0;
};

// Tests a simple animation from one desk to another.
TEST_F(RootWindowDeskSwitchAnimatorTest, SimpleAnimation) {
  InitAnimator(1, 2);
  test_api()->OnStartingDeskScreenshotTaken();
  test_api()->OnEndingDeskScreenshotTaken();

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

// Tests a chained animation where the replaced animation already has a
// screenshot layer stored.
TEST_F(RootWindowDeskSwitchAnimatorTest, ChainedAnimationNoNewScreenshot) {
  InitAnimator(1, 2);
  test_api()->OnStartingDeskScreenshotTaken();
  test_api()->OnEndingDeskScreenshotTaken();

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

// Tests a chained animation where we are adding an animation to the right of
// the current animating desks, causing the animation layer to shift left.
TEST_F(RootWindowDeskSwitchAnimatorTest, ChainedAnimationMovingLeft) {
  InitAnimator(1, 2);
  test_api()->OnStartingDeskScreenshotTaken();
  test_api()->OnEndingDeskScreenshotTaken();

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
  test_api()->OnEndingDeskScreenshotTaken();
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

// Tests a chained animation where we are adding an animation to the left of
// the current animating desks, causing the animation layer to shift right.
TEST_F(RootWindowDeskSwitchAnimatorTest, ChainedAnimationMovingRight) {
  InitAnimator(3, 2);
  test_api()->OnStartingDeskScreenshotTaken();
  test_api()->OnEndingDeskScreenshotTaken();

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
  test_api()->OnEndingDeskScreenshotTaken();
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

// Tests a complex animation which multiple animations are started and replaced.
TEST_F(RootWindowDeskSwitchAnimatorTest, MultipleReplacements) {
  InitAnimator(1, 2);
  test_api()->OnStartingDeskScreenshotTaken();
  test_api()->OnEndingDeskScreenshotTaken();

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
      test_api()->OnEndingDeskScreenshotTaken();
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

}  // namespace ash
