// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/single_scrollbar_animation_controller_thinning.h"

#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AtLeast;
using testing::Mock;
using testing::NiceMock;
using testing::_;

namespace cc {
namespace {

const float kIdleThicknessScale =
    SingleScrollbarAnimationControllerThinning::kIdleThicknessScale;
const float kMouseMoveDistanceToTriggerExpand =
    SingleScrollbarAnimationControllerThinning::
        kMouseMoveDistanceToTriggerExpand;
const float kMouseMoveDistanceToTriggerFadeIn =
    ScrollbarAnimationController::kMouseMoveDistanceToTriggerFadeIn;

class MockSingleScrollbarAnimationControllerClient
    : public ScrollbarAnimationControllerClient {
 public:
  explicit MockSingleScrollbarAnimationControllerClient(
      LayerTreeHostImpl* host_impl)
      : host_impl_(host_impl) {}
  ~MockSingleScrollbarAnimationControllerClient() override = default;

  ScrollbarSet ScrollbarsFor(ElementId scroll_element_id) const override {
    return host_impl_->ScrollbarsFor(scroll_element_id);
  }

  MOCK_METHOD2(PostDelayedScrollbarAnimationTask,
               void(base::OnceClosure start_fade, base::TimeDelta delay));
  MOCK_METHOD0(SetNeedsRedrawForScrollbarAnimation, void());
  MOCK_METHOD0(SetNeedsAnimateForScrollbarAnimation, void());
  MOCK_METHOD0(DidChangeScrollbarVisibility, void());

 private:
  LayerTreeHostImpl* host_impl_;
};

class SingleScrollbarAnimationControllerThinningTest
    : public LayerTreeImplTestBase,
      public testing::Test {
 public:
  SingleScrollbarAnimationControllerThinningTest() : client_(host_impl()) {}

 protected:
  const base::TimeDelta kThinningDuration = base::TimeDelta::FromSeconds(2);

  void SetUp() override {
    root_layer()->SetBounds(gfx::Size(100, 100));
    auto* scroll_layer = AddLayer<LayerImpl>();
    scroll_layer->SetBounds(gfx::Size(200, 200));
    scroll_layer->SetScrollable(gfx::Size(100, 100));
    scroll_layer->SetElementId(
        LayerIdToElementIdForTesting(scroll_layer->id()));

    const int kThumbThickness = 10;
    const int kTrackStart = 0;
    const int kTrackLength = 100;
    const bool kIsLeftSideVerticalScrollbar = false;

    scrollbar_layer_ = AddLayer<SolidColorScrollbarLayerImpl>(
        HORIZONTAL, kThumbThickness, kTrackStart, kIsLeftSideVerticalScrollbar);

    scrollbar_layer_->SetBounds(gfx::Size(kThumbThickness, kTrackLength));
    scrollbar_layer_->SetScrollElementId(scroll_layer->element_id());

    CopyProperties(root_layer(), scroll_layer);
    CreateTransformNode(scroll_layer);
    CreateScrollNode(scroll_layer);
    CopyProperties(scroll_layer, scrollbar_layer_);
    scrollbar_layer_->SetOffsetToTransformParent(gfx::Vector2dF(90, 0));
    CreateEffectNode(scrollbar_layer_).has_potential_opacity_animation = true;

    UpdateActiveTreeDrawProperties();

    scrollbar_controller_ = SingleScrollbarAnimationControllerThinning::Create(
        scroll_layer->element_id(), HORIZONTAL, &client_, kThinningDuration);
  }

  std::unique_ptr<SingleScrollbarAnimationControllerThinning>
      scrollbar_controller_;
  SolidColorScrollbarLayerImpl* scrollbar_layer_;
  NiceMock<MockSingleScrollbarAnimationControllerClient> client_;
};

// Return a point with given offset from the top-left of scrollbar.
gfx::PointF NearScrollbar(float offset_x, float offset_y) {
  gfx::PointF p(90, 0);
  p.Offset(offset_x, offset_y);
  return p;
}

// Check initialization of scrollbar. Should start thin.
TEST_F(SingleScrollbarAnimationControllerThinningTest, Idle) {
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

// Move the pointer near the scrollbar. Confirm it gets thick and narrow when
// moved away.
TEST_F(SingleScrollbarAnimationControllerThinningTest, MouseNear) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidMouseMove(NearScrollbar(-1, 0));
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Subsequent moves within the nearness threshold should not change anything.
  scrollbar_controller_->DidMouseMove(NearScrollbar(-2, 0));
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Now move away from thumb.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-kMouseMoveDistanceToTriggerExpand, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Move away from track.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-kMouseMoveDistanceToTriggerFadeIn, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

// Move the pointer over the scrollbar. Make sure it gets thick that it gets
// thin when moved away.
TEST_F(SingleScrollbarAnimationControllerThinningTest, MouseOver) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 0));
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Subsequent moves should not change anything.
  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 0));
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Moving off the scrollbar but still withing the "near" threshold should do
  // nothing.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-kMouseMoveDistanceToTriggerExpand + 1, 0));
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Now move away from thumb.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-kMouseMoveDistanceToTriggerExpand, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

// First move the pointer over the scrollbar off of it. Make sure the thinning
// animation kicked off in DidMouseMoveOffScrollbar gets overridden by the
// thickening animation in the DidMouseMove call.
TEST_F(SingleScrollbarAnimationControllerThinningTest,
       MouseNearThenAwayWhileAnimating) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 0));
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // This is tricky. The DidMouseLeave() is sent before the
  // subsequent DidMouseMove(), if the mouse moves in that direction.
  // This results in the thumb thinning. We want to make sure that when the
  // thumb starts expanding it doesn't first narrow to the idle thinness.
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidMouseLeave();
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Let the animation run half of the way through the thinning animation.
  time += kThinningDuration / 2;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f - (1.0f - kIdleThicknessScale) / 2.0f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Now we get a notification for the mouse moving over the scroller. The
  // animation is reset to the thickening direction but we won't start
  // thickening until the new animation catches up to the current thickness.
  scrollbar_controller_->DidMouseMove(NearScrollbar(-1, 0));
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f - (1.0f - kIdleThicknessScale) / 2.0f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Until we reach the half way point, the animation will have no effect.
  time += kThinningDuration / 4;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f - (1.0f - kIdleThicknessScale) / 2.0f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  time += kThinningDuration / 4;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f - (1.0f - kIdleThicknessScale) / 2.0f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // We're now at three quarters of the way through so it should now started
  // thickening again.
  time += kThinningDuration / 4;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale + 3 * (1.0f - kIdleThicknessScale) / 4.0f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // And all the way to the end.
  time += kThinningDuration / 4;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
}

// First move the pointer on the scrollbar, then press it, then away.
// Confirm that the bar gets thick. Then mouse up. Confirm that
// the bar gets thin.
TEST_F(SingleScrollbarAnimationControllerThinningTest,
       MouseCaptureAndReleaseOutOfBar) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Move over the scrollbar.
  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Capture
  scrollbar_controller_->DidMouseDown();
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Should stay thick for a while.
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);

  // Move outside the "near" threshold. Because the scrollbar is captured it
  // should remain thick.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-kMouseMoveDistanceToTriggerExpand, 0));
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Release.
  scrollbar_controller_->DidMouseUp();

  // Should become thin.
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

// First move the pointer on the scrollbar, then press it, then away.  Confirm
// that the bar gets thick. Then move point on the scrollbar and mouse up.
// Confirm that the bar stays thick.
TEST_F(SingleScrollbarAnimationControllerThinningTest,
       MouseCaptureAndReleaseOnBar) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Move over scrollbar.
  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Capture. Nothing should change.
  scrollbar_controller_->DidMouseDown();
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Move away from scrollbar. Nothing should change.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(kMouseMoveDistanceToTriggerExpand, 0));
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Move over scrollbar and release. Since we're near the scrollbar, it should
  // remain thick.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-kMouseMoveDistanceToTriggerExpand + 1, 0));
  scrollbar_controller_->DidMouseUp();
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  time += base::TimeDelta::FromSeconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
}

// Tests that the thickening/thinning effects are animated.
TEST_F(SingleScrollbarAnimationControllerThinningTest, ThicknessAnimated) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  // Move mouse near scrollbar. Test that at half the duration time, the
  // thickness is half way through its animation.
  scrollbar_controller_->DidMouseMove(NearScrollbar(-1, 0));
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  time += kThinningDuration / 2;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale + (1.0f - kIdleThicknessScale) / 2.0f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  time += kThinningDuration / 2;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Move mouse away from scrollbar. Same check.
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-kMouseMoveDistanceToTriggerExpand, 0));
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += kThinningDuration / 2;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f - (1.0f - kIdleThicknessScale) / 2.0f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  time += kThinningDuration / 2;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

}  // namespace
}  // namespace cc
