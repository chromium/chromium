// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/single_scrollbar_animation_controller_thinning.h"

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/test/geometry_util.h"

using ::testing::_;
using ::testing::Bool;
using ::testing::Mock;
using ::testing::NiceMock;

namespace cc {
namespace {

// Redefinition from ui/native_theme/overlay_scrollbar_constants_aura.h
const float kIdleThicknessScale = 0.4f;

class MockSingleScrollbarAnimationControllerClient
    : public ScrollbarAnimationControllerClient {
 public:
  MockSingleScrollbarAnimationControllerClient(LayerTreeHostImpl* host_impl,
                                               bool is_fluent)
      : host_impl_(host_impl), is_fluent_(is_fluent) {}
  ~MockSingleScrollbarAnimationControllerClient() override = default;

  ScrollbarSet ScrollbarsFor(ElementId scroll_element_id) const override {
    return host_impl_->ScrollbarsFor(scroll_element_id);
  }
  bool IsFluentOverlayScrollbar() const override { return is_fluent_; }

  MOCK_METHOD2(PostDelayedScrollbarAnimationTask,
               void(base::OnceClosure start_fade, base::TimeDelta delay));
  MOCK_METHOD0(SetNeedsRedrawForScrollbarAnimation, void());
  MOCK_METHOD0(SetNeedsAnimateForScrollbarAnimation, void());
  MOCK_METHOD0(DidChangeScrollbarVisibility, void());

 private:
  raw_ptr<LayerTreeHostImpl> host_impl_;
  bool is_fluent_;
};

class SingleScrollbarAnimationControllerThinningTest
    : public LayerTreeImplTestBase,
      public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  explicit SingleScrollbarAnimationControllerThinningTest(
      bool is_fluent = GetParam())
      : client_(host_impl(), is_fluent) {}

 protected:
  const base::TimeDelta kThinningDuration = base::Seconds(2);
  const int kThumbThickness = 10;

  void SetUp() override {
    root_layer()->SetBounds(gfx::Size(100, 100));
    auto* scroll_layer = AddLayerInActiveTree<LayerImpl>();
    scroll_layer->SetBounds(gfx::Size(200, 200));
    scroll_layer->SetElementId(
        LayerIdToElementIdForTesting(scroll_layer->id()));

    const int kTrackStart = 0;
    const int kTrackLength = 100;
    const bool kIsLeftSideVerticalScrollbar = false;

    scrollbar_layer_ = AddLayerInActiveTree<SolidColorScrollbarLayerImpl>(
        ScrollbarOrientation::kVertical, kThumbThickness, kTrackStart,
        kIsLeftSideVerticalScrollbar);

    scrollbar_layer_->SetBounds(gfx::Size(kThumbThickness, kTrackLength));
    scrollbar_layer_->SetScrollElementId(scroll_layer->element_id());

    CopyProperties(root_layer(), scroll_layer);
    CreateTransformNode(scroll_layer);
    CreateScrollNode(scroll_layer, gfx::Size(100, 100));
    CopyProperties(scroll_layer, scrollbar_layer_);
    scrollbar_layer_->SetOffsetToTransformParent(gfx::Vector2dF(90, 0));
    CreateEffectNode(scrollbar_layer_).has_potential_opacity_animation = true;

    host_impl()->active_tree()->UpdateAllScrollbarGeometriesForTesting();
    UpdateActiveTreeDrawProperties();

    scrollbar_controller_ = SingleScrollbarAnimationControllerThinning::Create(
        scroll_layer->element_id(), ScrollbarOrientation::kVertical, &client_,
        kThinningDuration, kIdleThicknessScale);
    mouse_move_distance_to_trigger_fade_in_ =
        scrollbar_controller_->MouseMoveDistanceToTriggerFadeIn();
    mouse_move_distance_to_trigger_expand_ =
        scrollbar_controller_->MouseMoveDistanceToTriggerExpand();
  }

  float mouse_move_distance_to_trigger_fade_in_;
  float mouse_move_distance_to_trigger_expand_;
  std::unique_ptr<SingleScrollbarAnimationControllerThinning>
      scrollbar_controller_;
  raw_ptr<SolidColorScrollbarLayerImpl> scrollbar_layer_;
  NiceMock<MockSingleScrollbarAnimationControllerClient> client_;
};

// Return a point with given offset from the top-left of scrollbar.
gfx::PointF NearScrollbar(float offset_x, float offset_y) {
  gfx::PointF p(90, 0);
  p.Offset(offset_x, offset_y);
  return p;
}

class SingleScrollbarAnimationControllerThinningAuraTest
    : public SingleScrollbarAnimationControllerThinningTest {
 public:
  SingleScrollbarAnimationControllerThinningAuraTest()
      : SingleScrollbarAnimationControllerThinningTest(/*is_fluent=*/false) {}
};

class SingleScrollbarAnimationControllerThinningFluentTest
    : public SingleScrollbarAnimationControllerThinningTest {
 public:
  SingleScrollbarAnimationControllerThinningFluentTest()
      : SingleScrollbarAnimationControllerThinningTest(/*is_fluent=*/true) {}
};

// Check initialization of scrollbar. Should start thin.
TEST_P(SingleScrollbarAnimationControllerThinningTest, Idle) {
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

// Move the pointer near the scrollbar. Confirm it gets thick and narrow when
// moved away.
TEST_P(SingleScrollbarAnimationControllerThinningTest, MouseNear) {
  base::TimeTicks time;
  time += base::Seconds(1);

  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-mouse_move_distance_to_trigger_expand_, 0));
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Subsequent moves within the nearness threshold should not change anything.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-mouse_move_distance_to_trigger_expand_ + 1, 0));
  scrollbar_controller_->Animate(time);
  time += base::Seconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Now move away from thumb.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-mouse_move_distance_to_trigger_expand_ - 1, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Move away from track.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-mouse_move_distance_to_trigger_fade_in_ - 1, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

// Move the pointer over the scrollbar. Make sure it gets thick that it gets
// thin when moved away.
TEST_P(SingleScrollbarAnimationControllerThinningTest, MouseOver) {
  base::TimeTicks time;
  time += base::Seconds(1);

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
  time += base::Seconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Moving off the scrollbar but still withing the "near" threshold should do
  // nothing.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-mouse_move_distance_to_trigger_expand_, 0));
  scrollbar_controller_->Animate(time);
  time += base::Seconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Now move away from thumb.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-mouse_move_distance_to_trigger_expand_ - 1, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

// First move the pointer over the scrollbar off of it. Make sure the thinning
// animation kicked off in DidMouseMoveOffScrollbar gets overridden by the
// thickening animation in the DidMouseMove call.
TEST_F(SingleScrollbarAnimationControllerThinningAuraTest,
       MouseNearThenAwayWhileAnimating) {
  base::TimeTicks time;
  time += base::Seconds(1);

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
  time += base::Seconds(1);
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
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-mouse_move_distance_to_trigger_expand_, 0));
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
TEST_P(SingleScrollbarAnimationControllerThinningTest,
       MouseCaptureAndReleaseOutOfBar) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Move over the scrollbar.
  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Capture
  scrollbar_controller_->DidMouseDown();
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Should stay thick for a while.
  time += base::Seconds(10);
  scrollbar_controller_->Animate(time);

  // Move outside the "near" threshold. Because the scrollbar is captured it
  // should remain thick.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-mouse_move_distance_to_trigger_expand_ - 1, 0));
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Release.
  scrollbar_controller_->DidMouseUp();

  // Should become thin.
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  // Fluent scrollbars dont thin out on mouse leave, they go straight to the
  // scrollbar disappearance animation (via ScrolbarAnimationController).
  if (client_.IsFluentOverlayScrollbar()) {
    EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
    return;
  }
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

// First move the pointer on the scrollbar, then press it, then away.  Confirm
// that the bar gets thick. Then move point on the scrollbar and mouse up.
// Confirm that the bar stays thick.
TEST_P(SingleScrollbarAnimationControllerThinningTest,
       MouseCaptureAndReleaseOnBar) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Move over scrollbar.
  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Capture. Nothing should change.
  scrollbar_controller_->DidMouseDown();
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  time += base::Seconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Move away from scrollbar. Nothing should change.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(mouse_move_distance_to_trigger_expand_ + 1, 0));
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  time += base::Seconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Move over scrollbar and release. Since we're near the scrollbar, it should
  // remain thick.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-mouse_move_distance_to_trigger_expand_, 0));
  scrollbar_controller_->DidMouseUp();
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  time += base::Seconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
}

// Tests that the thickening/thinning effects are animated.
TEST_P(SingleScrollbarAnimationControllerThinningTest, ThicknessAnimated) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Move mouse near scrollbar. Test that at half the duration time, the
  // thickness is half way through its animation.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-mouse_move_distance_to_trigger_expand_, 0));
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
  time += base::Seconds(1);
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-mouse_move_distance_to_trigger_expand_ - 1, 0));
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

// Make sure the Fluent scrollbar transitions to the full mode (thick) after
// moving the mouse over scrollbar track and get back to the minimal mode (thin)
// when moved away both inside and outside the layer.
TEST_F(SingleScrollbarAnimationControllerThinningFluentTest, MouseOverTrack) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Move the mouse over the scrollbar track.
  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 75));
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to the full mode.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Subsequent moves should not change anything.
  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 75));
  scrollbar_controller_->Animate(time);
  time += base::Seconds(10);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Moving away from the scrollbar track should trigger the transition to the
  // minimal mode.
  scrollbar_controller_->DidMouseMove(NearScrollbar(-1, 75));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Move the mouse over the scrollbar track again. Scrollbar should be in the
  // full mode.
  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 75));
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Moving away from the scrollbar track out of the layer should also
  // trigger the transition to the minimal mode.
  scrollbar_controller_->DidMouseMove(NearScrollbar(kThumbThickness + 1, 75));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

// Check that Fluent overlay scrollbar doesn't get thicker and doesn't get
// shown when mousing over it while it is hidden.
TEST_F(SingleScrollbarAnimationControllerThinningFluentTest,
       MouseOverHiddenBar) {
  // Scrollbars opacity value is 1.f on these tests start up. Set it 0 to
  // simulate a hidden scrollbar
  scrollbar_layer_->SetOverlayScrollbarLayerOpacityAnimated(
      0.f, /*fade_out_animation=*/false);
  EXPECT_FLOAT_EQ(0.f, scrollbar_layer_->Opacity());

  // Move mouse on top of scrollbar.
  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 0));

  // Tick the animations and ensure its thickness remains thin.
  base::TimeTicks time;
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
  scrollbar_controller_->Animate(time + kThinningDuration);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(0.f, scrollbar_layer_->Opacity());
}

// Check that Fluent overlay scrollbar doesn't get thinner when released outside
// of the scrollbar area with tickmarks showing.
TEST_F(SingleScrollbarAnimationControllerThinningFluentTest,
       ShowTickmarksAndReleaseOutsideBar) {
  EXPECT_CALL(client_, SetNeedsAnimateForScrollbarAnimation()).Times(0);
  // Simulate tickmarks showing. Scrollbar should be visible and fully thick.
  scrollbar_controller_->UpdateTickmarksVisibility(true);
  EXPECT_FLOAT_EQ(1.f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1.f, scrollbar_layer_->Opacity());

  // Move mouse on top of scrollbar and capture it.
  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 0));
  scrollbar_controller_->DidMouseDown();
  EXPECT_TRUE(scrollbar_controller_->captured());

  // Move mouse away from scrollbar and release it.
  scrollbar_controller_->DidMouseMove(
      NearScrollbar(-mouse_move_distance_to_trigger_fade_in_ - 1, 0));
  scrollbar_controller_->DidMouseUp();

  // Pass time and ensure scrollbar remains visible and fully thick.
  base::TimeTicks time;
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  scrollbar_controller_->Animate(time + kThinningDuration);
  EXPECT_FLOAT_EQ(1.f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1.f, scrollbar_layer_->Opacity());
}

// Check that Fluent overlay scrollbar doesn't get thinner when tickmarks are
// showing and the mouse moves over the scrollbar and then leaves the scrollable
// area.
TEST_F(SingleScrollbarAnimationControllerThinningFluentTest,
       ShowTickmarksAndMouseLeave) {
  EXPECT_CALL(client_, SetNeedsAnimateForScrollbarAnimation()).Times(0);
  // Simulate tickmarks showing. Scrollbar should be visible and fully thick.
  scrollbar_controller_->UpdateTickmarksVisibility(true);
  EXPECT_FLOAT_EQ(1.f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1.f, scrollbar_layer_->Opacity());

  // Move mouse on top of scrollbar and leave scrollable area.
  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 0));
  scrollbar_controller_->DidMouseLeave();

  // Pass time and ensure scrollbar remains visible and fully thick.
  base::TimeTicks time;
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  scrollbar_controller_->Animate(time + kThinningDuration);
  EXPECT_FLOAT_EQ(1.f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1.f, scrollbar_layer_->Opacity());
}

// Check that if the mouse leaves while thickening animation is happening, the
// animation finishes as a thinning one.
TEST_F(SingleScrollbarAnimationControllerThinningFluentTest,
       ThickeningToThinningOnMouseLeave) {
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
  base::TimeTicks time;
  time += base::Seconds(1);

  // Move mouse on top of scrollbar and animate half of the thickening
  // animation.
  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 0));
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
  time += kThinningDuration / 2.f;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale + (1.f - kIdleThicknessScale) / 2.f,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Mouse leaves, the controller should make the animation be a thinning one.
  scrollbar_controller_->DidMouseLeave();
  scrollbar_controller_->Animate(time);
  time += kThinningDuration / 2.f;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale + (1.f - kIdleThicknessScale) / 2.f,
                  scrollbar_layer_->thumb_thickness_scale_factor());
  time += kThinningDuration / 2.f;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

// Check that if the mouse leaves after the thickening animation is complete
// no thinning animation gets queued.
TEST_F(SingleScrollbarAnimationControllerThinningFluentTest,
       DoesntAnimateOnFullModeMouseLeave) {
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
  base::TimeTicks time;
  time += base::Seconds(1);

  // Move mouse on top of scrollbar and animate the full animation duration.
  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Mouse leaves, the controller should not queue an animation.
  scrollbar_controller_->DidMouseLeave();
  EXPECT_FALSE(scrollbar_controller_->Animate(time));
}

// Test that the last pointer location variable is set on DidMouseMove calls and
// mouse position variables are correctly updated in DidScrollUpdate() calls.
TEST_P(SingleScrollbarAnimationControllerThinningTest,
       HoverTrackAndMoveThumbUnderPointer) {
  EXPECT_POINTF_EQ(
      gfx::PointF(-1, -1),
      scrollbar_controller_->device_viewport_last_pointer_location());

  // Move mouse on top of the scrollbar track but not the thumb, and verify
  // that all variables are correctly set.
  gfx::PointF near_scrollbar = NearScrollbar(0, 90);
  scrollbar_controller_->DidMouseMove(near_scrollbar);
  EXPECT_POINTF_EQ(
      near_scrollbar,
      scrollbar_controller_->device_viewport_last_pointer_location());
  EXPECT_FALSE(scrollbar_controller_->mouse_is_near_scrollbar_thumb());
  EXPECT_FALSE(scrollbar_controller_->mouse_is_over_scrollbar_thumb());
  EXPECT_TRUE(scrollbar_controller_->mouse_is_near_scrollbar());
  scrollbar_controller_->DidMouseDown();
  EXPECT_FALSE(scrollbar_controller_->captured());

  // Move the thumb to the end of the track so that the pointer is located over
  // it.
  EXPECT_TRUE(scrollbar_layer_->SetCurrentPos(100));
  scrollbar_controller_->DidScrollUpdate();
  EXPECT_TRUE(scrollbar_controller_->mouse_is_near_scrollbar_thumb());
  EXPECT_TRUE(scrollbar_controller_->mouse_is_over_scrollbar_thumb());
  EXPECT_TRUE(scrollbar_controller_->mouse_is_near_scrollbar());

  // Clicking now should capture the thumb.
  scrollbar_controller_->DidMouseDown();
  EXPECT_TRUE(scrollbar_controller_->captured());
}

// Test that DidScrollUpdate correctly queues thinning animations when the thumb
// moves under the pointer and when it moves away from it.
TEST_F(SingleScrollbarAnimationControllerThinningAuraTest,
       DidScrollUpdateQueuesAnimations) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Move mouse on top of the scrollbar track but not the thumb. No animation
  // should be queued.
  scrollbar_controller_->DidMouseMove(NearScrollbar(0, 90));
  EXPECT_FALSE(scrollbar_controller_->Animate(time));
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());

  // Move the thumb to the end of the track so that the pointer is located over
  // it.
  EXPECT_TRUE(scrollbar_layer_->SetCurrentPos(100));
  scrollbar_controller_->DidScrollUpdate();
  EXPECT_TRUE(scrollbar_controller_->Animate(time));

  // The thumb should animate and become thick.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Move the layer's thumb to its starting position.
  EXPECT_TRUE(scrollbar_layer_->SetCurrentPos(0));
  scrollbar_controller_->DidScrollUpdate();
  scrollbar_controller_->Animate(time);

  // The thumb should become thin as the mouse is no longer on top of it.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  scrollbar_layer_->thumb_thickness_scale_factor());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SingleScrollbarAnimationControllerThinningTest,
                         Bool());

}  // namespace
}  // namespace cc
