// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scrollbar_animation_controller.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using ::testing::Bool;
using testing::Mock;
using testing::NiceMock;

namespace cc {
namespace {

// Redefinition from ui/native_theme/overlay_scrollbar_constants_aura.h
const float kIdleThicknessScale = 0.4f;
const int kThumbThickness = 10;

class MockScrollbarAnimationControllerClient
    : public ScrollbarAnimationControllerClient {
 public:
  MockScrollbarAnimationControllerClient(LayerTreeHostImpl* host_impl,
                                         bool is_fluent)
      : host_impl_(host_impl), is_fluent_(is_fluent) {}
  ~MockScrollbarAnimationControllerClient() override = default;

  void PostDelayedScrollbarAnimationTask(base::OnceClosure start_fade,
                                         base::TimeDelta delay) override {
    start_fade_ = std::move(start_fade);
    delay_ = delay;
  }
  void SetNeedsRedrawForScrollbarAnimation() override {}
  void SetNeedsAnimateForScrollbarAnimation() override {}
  ScrollbarSet ScrollbarsFor(ElementId scroll_element_id) const override {
    return host_impl_->ScrollbarsFor(scroll_element_id);
  }
  MOCK_METHOD0(DidChangeScrollbarVisibility, void());
  bool IsFluentOverlayScrollbar() const override { return is_fluent_; }

  base::OnceClosure& start_fade() { return start_fade_; }
  base::TimeDelta& delay() { return delay_; }

 private:
  base::OnceClosure start_fade_;
  base::TimeDelta delay_;
  raw_ptr<LayerTreeHostImpl> host_impl_;
  bool is_fluent_;
};

class ScrollbarAnimationControllerOverlayTest
    : public LayerTreeImplTestBase,
      public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  explicit ScrollbarAnimationControllerOverlayTest(bool is_fluent = GetParam())
      : client_(host_impl(), is_fluent) {}

  void ExpectScrollbarsOpacity(float opacity) {
    EXPECT_FLOAT_EQ(opacity, v_scrollbar_layer_->Opacity());
    EXPECT_FLOAT_EQ(opacity, h_scrollbar_layer_->Opacity());
  }

  void UpdateScrollbarGeometriesAndDrawProperties() {
    host_impl()->active_tree()->UpdateAllScrollbarGeometriesForTesting();
    UpdateActiveTreeDrawProperties();
  }

 protected:
  const base::TimeDelta kFadeDelay = base::Seconds(4);
  const base::TimeDelta kFadeDuration = base::Seconds(3);
  const base::TimeDelta kThinningDuration = base::Seconds(2);

  void SetUp() override {
    const int kTrackStart = 0;
    const int kTrackLength = 100;
    const bool kIsLeftSideVerticalScrollbar = false;

    scroll_layer_ = AddLayerInActiveTree<LayerImpl>();
    h_scrollbar_layer_ = AddLayerInActiveTree<SolidColorScrollbarLayerImpl>(
        ScrollbarOrientation::kHorizontal, kThumbThickness, kTrackStart,
        kIsLeftSideVerticalScrollbar);
    v_scrollbar_layer_ = AddLayerInActiveTree<SolidColorScrollbarLayerImpl>(
        ScrollbarOrientation::kVertical, kThumbThickness, kTrackStart,
        kIsLeftSideVerticalScrollbar);
    SetElementIdsForTesting();

    clip_layer_ = root_layer();
    clip_layer_->SetBounds(gfx::Size(100, 100));

    scroll_layer_->SetBounds(gfx::Size(200, 200));
    CopyProperties(clip_layer_, scroll_layer_);
    CreateTransformNode(scroll_layer_);
    CreateScrollNode(scroll_layer_, gfx::Size(100, 100));

    v_scrollbar_layer_->SetBounds(gfx::Size(kThumbThickness, kTrackLength));
    v_scrollbar_layer_->SetScrollElementId(scroll_layer_->element_id());
    CopyProperties(scroll_layer_, v_scrollbar_layer_);
    v_scrollbar_layer_->SetOffsetToTransformParent(gfx::Vector2dF(90, 0));
    auto& v_scrollbar_effect = CreateEffectNode(v_scrollbar_layer_);
    v_scrollbar_effect.opacity = 0.f;
    v_scrollbar_effect.has_potential_opacity_animation = true;

    h_scrollbar_layer_->SetBounds(gfx::Size(kTrackLength, kThumbThickness));
    h_scrollbar_layer_->SetScrollElementId(scroll_layer_->element_id());
    CopyProperties(scroll_layer_, h_scrollbar_layer_);
    h_scrollbar_layer_->SetOffsetToTransformParent(gfx::Vector2dF(0, 90));
    auto& h_scrollbar_effect = CreateEffectNode(h_scrollbar_layer_);
    h_scrollbar_effect.opacity = 0.f;
    h_scrollbar_effect.has_potential_opacity_animation = true;

    UpdateScrollbarGeometriesAndDrawProperties();

    scrollbar_controller_ = ScrollbarAnimationController::
        CreateScrollbarAnimationControllerAuraOverlay(
            scroll_layer_->element_id(), &client_, kFadeDelay, kFadeDuration,
            kThinningDuration, 0.0f, kIdleThicknessScale);
    v_scrollbar_layer_->SetCurrentPos(0);
    h_scrollbar_layer_->SetCurrentPos(0);
    mouse_move_distance_to_trigger_fade_in_ =
        scrollbar_controller_
            ->GetScrollbarAnimationController(ScrollbarOrientation::kVertical)
            .MouseMoveDistanceToTriggerFadeIn();
    mouse_move_distance_to_trigger_expand_ =
        scrollbar_controller_
            ->GetScrollbarAnimationController(ScrollbarOrientation::kVertical)
            .MouseMoveDistanceToTriggerExpand();
  }

  // Return a point with given offset from the top-left of vertical scrollbar.
  gfx::PointF NearVerticalScrollbarBegin(float offset_x, float offset_y) {
    gfx::PointF p(90, 0);
    p.Offset(offset_x, offset_y);
    return p;
  }

  // Return a point with given offset from the bottom-left of vertical
  // scrollbar.
  gfx::PointF NearVerticalScrollbarEnd(float offset_x, float offset_y) {
    gfx::PointF p(90, 90);
    p.Offset(offset_x, offset_y);
    return p;
  }

  // Return a point with given offset from the top-left of horizontal scrollbar.
  gfx::PointF NearHorizontalScrollbarBegin(float offset_x, float offset_y) {
    gfx::PointF p(0, 90);
    p.Offset(offset_x, offset_y);
    return p;
  }

  float mouse_move_distance_to_trigger_fade_in_;
  float mouse_move_distance_to_trigger_expand_;
  std::unique_ptr<ScrollbarAnimationController> scrollbar_controller_;
  raw_ptr<LayerImpl> clip_layer_;
  raw_ptr<LayerImpl> scroll_layer_;
  raw_ptr<SolidColorScrollbarLayerImpl> v_scrollbar_layer_;
  raw_ptr<SolidColorScrollbarLayerImpl> h_scrollbar_layer_;
  NiceMock<MockScrollbarAnimationControllerClient> client_;
};

class ScrollbarAnimationControllerAuraOverlayTest
    : public ScrollbarAnimationControllerOverlayTest {
 public:
  ScrollbarAnimationControllerAuraOverlayTest()
      : ScrollbarAnimationControllerOverlayTest(/*is_fluent=*/false) {}
};

class ScrollbarAnimationControllerFluentOverlayTest
    : public ScrollbarAnimationControllerOverlayTest {
 public:
  ScrollbarAnimationControllerFluentOverlayTest()
      : ScrollbarAnimationControllerOverlayTest(/*is_fluent=*/true) {}
};

// Check initialization of scrollbar. Should start off invisible and thin.
TEST_P(ScrollbarAnimationControllerOverlayTest, Idle) {
  ExpectScrollbarsOpacity(0);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Check that scrollbar appears again when the layer becomes scrollable.
TEST_P(ScrollbarAnimationControllerOverlayTest, AppearOnResize) {
  base::TimeTicks time;
  time += base::Seconds(1);

  scrollbar_controller_->DidScrollUpdate();
  ExpectScrollbarsOpacity(1);

  // Make the Layer non-scrollable, scrollbar disappears.
  clip_layer_->SetBounds(gfx::Size(200, 200));
  GetScrollNode(scroll_layer_.get())->container_bounds = gfx::Size(200, 200);
  UpdateScrollbarGeometriesAndDrawProperties();
  scrollbar_controller_->DidScrollUpdate();
  ExpectScrollbarsOpacity(0);

  // Make the layer scrollable, scrollbar appears again.
  clip_layer_->SetBounds(gfx::Size(100, 100));
  GetScrollNode(scroll_layer_.get())->container_bounds = gfx::Size(100, 100);
  UpdateScrollbarGeometriesAndDrawProperties();
  scrollbar_controller_->DidScrollUpdate();
  ExpectScrollbarsOpacity(1);
}

// Check that scrollbar disappears when the layer becomes non-scrollable.
TEST_P(ScrollbarAnimationControllerOverlayTest, HideOnResize) {
  base::TimeTicks time;
  time += base::Seconds(1);

  EXPECT_EQ(gfx::Size(200, 200), scroll_layer_->bounds());

  // Shrink along X axis, horizontal scrollbar should appear.
  clip_layer_->SetBounds(gfx::Size(100, 200));
  EXPECT_EQ(gfx::Size(100, 200), clip_layer_->bounds());
  GetScrollNode(scroll_layer_.get())->container_bounds = gfx::Size(100, 200);
  UpdateScrollbarGeometriesAndDrawProperties();

  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->Opacity());

  // Shrink along Y axis and expand along X, horizontal scrollbar
  // should disappear.
  clip_layer_->SetBounds(gfx::Size(200, 100));
  EXPECT_EQ(gfx::Size(200, 100), clip_layer_->bounds());
  GetScrollNode(scroll_layer_.get())->container_bounds = gfx::Size(200, 100);
  UpdateScrollbarGeometriesAndDrawProperties();

  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FLOAT_EQ(0.0f, h_scrollbar_layer_->Opacity());
}

// Scroll content. Confirm the scrollbar appears and fades out.
TEST_P(ScrollbarAnimationControllerOverlayTest, BasicAppearAndFadeOut) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Scrollbar should be invisible.
  ExpectScrollbarsOpacity(0);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());

  // Scrollbar should appear only on scroll update.
  ExpectScrollbarsOpacity(0);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());

  scrollbar_controller_->DidScrollUpdate();
  ExpectScrollbarsOpacity(1);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());

  ExpectScrollbarsOpacity(1);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  std::move(client_.start_fade()).Run();

  // Scrollbar should fade out over kFadeDuration.
  scrollbar_controller_->Animate(time);
  time += kFadeDuration;
  scrollbar_controller_->Animate(time);

  ExpectScrollbarsOpacity(0);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
}

// Confirm the scrollbar appears by WillUpdateScroll and fade out.
TEST_P(ScrollbarAnimationControllerOverlayTest,
       BasicAppearByWillUpdateScrollThenFadeOut) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Scrollbar should be invisible.
  ExpectScrollbarsOpacity(0);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());

  // Scrollbar should appear when scroll will update.
  scrollbar_controller_->WillUpdateScroll();
  ExpectScrollbarsOpacity(1);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  std::move(client_.start_fade()).Run();

  // Scrollbar should fade out over kFadeDuration.
  scrollbar_controller_->Animate(time);
  time += kFadeDuration;
  scrollbar_controller_->Animate(time);

  ExpectScrollbarsOpacity(0);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
}

// Scroll content. Move the mouse near the scrollbar track but not near thumb
// and confirm it stay thin. Move the mouse near the scrollbar thumb and
// confirm it becomes thick.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest,
       MoveNearTrackThenNearThumb) {
  base::TimeTicks time;
  time += base::Seconds(1);

  scrollbar_controller_->DidScrollUpdate();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse near the vertical scrollbar track. This should cancel
  // the currently queued fading animation and stay scrollbar thin.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarEnd(-1, 0));
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().IsCancelled());

  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);

  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(-1, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);

  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarEnd(-1, 0));
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().IsCancelled());

  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);

  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Scroll content. Move the mouse near the scrollbar thumb and confirm it
// becomes thick. Ensure it remains visible as long as the mouse is near the
// scrollbar.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, MoveNearAndDontFadeOut) {
  base::TimeTicks time;
  time += base::Seconds(1);

  scrollbar_controller_->DidScrollUpdate();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse near the vertical scrollbar thumb. This should cancel
  // the currently queued fading animation and start animating thickness.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(-1, 0));
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().IsCancelled());

  // Vertical scrollbar should become thick.
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Mouse is still near the Scrollbar. Once the thickness animation is
  // complete, the queued delayed fade out animation should be either cancelled
  // or null.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Scroll content. Move the mouse over the scrollbar and confirm it becomes
// thick. Ensure it remains visible as long as the mouse is over the scrollbar.
TEST_P(ScrollbarAnimationControllerOverlayTest, MoveOverAndDontFadeOut) {
  base::TimeTicks time;
  time += base::Seconds(1);

  scrollbar_controller_->DidScrollUpdate();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse over the vertical scrollbar thumb. This should cancel
  // the currently queued fading animation and start animating thickness.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(0, 0));
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().IsCancelled());

  // Vertical scrollbar should become thick.
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Mouse is still over the Scrollbar. Once the thickness animation is
  // complete, the queued delayed fade out animation should be either cancelled
  // or null.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Make sure a scrollbar captured before the thickening animation doesn't try
// to fade out.
TEST_P(ScrollbarAnimationControllerOverlayTest,
       DontFadeWhileCapturedBeforeThick) {
  base::TimeTicks time;
  time += base::Seconds(1);

  scrollbar_controller_->DidScrollUpdate();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());

  // Now move the mouse over the vertical scrollbar thumb and capture it. It
  // should become thick without need for an animation.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(0, 0));
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // The fade out animation should have been cleared or cancelled.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Make sure a scrollbar captured then move mouse away doesn't try to fade out.
TEST_P(ScrollbarAnimationControllerOverlayTest, DontFadeWhileCapturedThenAway) {
  base::TimeTicks time;
  time += base::Seconds(1);

  scrollbar_controller_->DidScrollUpdate();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());

  // Now move the mouse over the vertical scrollbar and capture it. It should
  // become thick without need for an animation.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(0, 0));
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // The fade out animation should have been cleared or cancelled.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Then move mouse away, The fade out animation should have been cleared or
  // cancelled.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(
      -mouse_move_distance_to_trigger_expand_ - 1, 0));

  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Make sure a scrollbar captured after a thickening animation doesn't try to
// fade out.
TEST_P(ScrollbarAnimationControllerOverlayTest, DontFadeWhileCaptured) {
  base::TimeTicks time;
  time += base::Seconds(1);

  scrollbar_controller_->DidScrollUpdate();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse over the vertical scrollbar thumb and animate it until
  // it's thick.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(0, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Since the mouse is over the scrollbar, it should either clear or cancel the
  // queued fade.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Make sure the queued fade out animation is still null or cancelled after
  // capturing the scrollbar.
  scrollbar_controller_->DidMouseDown();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Make sure releasing a captured scrollbar when the mouse isn't near it causes
// the scrollbar to fade out.
TEST_P(ScrollbarAnimationControllerOverlayTest, FadeAfterReleasedFar) {
  base::TimeTicks time;
  time += base::Seconds(1);

  scrollbar_controller_->DidScrollUpdate();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse over the vertical scrollbar thumb and capture it.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(0, 0));
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Since the mouse is still near the scrollbar, the queued fade should be
  // either null or cancelled.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Now move the mouse away from the scrollbar and release it.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(
      -mouse_move_distance_to_trigger_fade_in_ - 1, 0));
  scrollbar_controller_->DidMouseUp();

  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  if (!client_.IsFluentOverlayScrollbar()) {
    EXPECT_FLOAT_EQ(kIdleThicknessScale,
                    v_scrollbar_layer_->thumb_thickness_scale_factor());
    EXPECT_FLOAT_EQ(kIdleThicknessScale,
                    h_scrollbar_layer_->thumb_thickness_scale_factor());
  }

  // The thickness animation is complete, a fade out must be queued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
}

// Make sure releasing a captured scrollbar when the mouse is near/over it,
// doesn't cause the scrollbar to fade out.
TEST_P(ScrollbarAnimationControllerOverlayTest, DontFadeAfterReleasedNear) {
  base::TimeTicks time;
  time += base::Seconds(1);

  scrollbar_controller_->DidScrollUpdate();

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());

  // Now move the mouse over the vertical scrollbar thumb and capture it.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(0, 0));
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Since the mouse is over the scrollbar, the queued fade must be either
  // null or cancelled.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Mouse is still near the scrollbar, releasing it shouldn't do anything.
  scrollbar_controller_->DidMouseUp();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Make sure moving near a scrollbar while it's fading out causes it to reset
// the opacity and thicken.
TEST_P(ScrollbarAnimationControllerOverlayTest, MoveNearScrollbarWhileFading) {
  base::TimeTicks time;
  time += base::Seconds(1);

  scrollbar_controller_->DidScrollUpdate();

  // A fade out animation should have been enqueued. Start it.
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  std::move(client_.start_fade()).Run();

  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);

  // Proceed half way through the fade out animation.
  time += kFadeDuration / 2;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(.5f);

  // Now move the mouse near the vertical scrollbar thumb. It should reset
  // opacity to 1 instantly and start animating to thick.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(0, 0));
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Make sure we can't capture scrollbar that's completely faded out.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, TestCantCaptureWhenFaded) {
  base::TimeTicks time;
  time += base::Seconds(1);

  scrollbar_controller_->DidScrollUpdate();

  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  std::move(client_.start_fade()).Run();
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);

  // Fade the scrollbar out completely.
  time += kFadeDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(0);

  // Move mouse over the vertical scrollbar thumb. It shouldn't thicken the
  // scrollbar since it's completely faded out.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(0, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(0);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  client_.start_fade().Reset();

  // Now try to capture the scrollbar. It shouldn't do anything since it's
  // completely faded out.
  scrollbar_controller_->DidMouseDown();
  ExpectScrollbarsOpacity(0);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(client_.start_fade().is_null());

  // Similarly, releasing the scrollbar should have no effect but trigger a fade
  // in.
  scrollbar_controller_->DidMouseUp();
  ExpectScrollbarsOpacity(0);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // An fade in animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  EXPECT_EQ(kFadeDelay, client_.delay());

  // Play the delay animation.
  std::move(client_.start_fade()).Run();

  scrollbar_controller_->Animate(time);
  time += kFadeDuration;
  scrollbar_controller_->Animate(time);

  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
}

// Initiate a scroll when the pointer is already near the scrollbar. It should
// appear thick and remain thick.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, ScrollWithMouseNear) {
  base::TimeTicks time;
  time += base::Seconds(1);

  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(-1, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;

  // Since the scrollbar isn't visible yet (because we haven't scrolled), we
  // shouldn't have applied the thickening.
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  scrollbar_controller_->DidScrollUpdate();

  // Now that we've received a scroll, we should be thick without an animation.
  ExpectScrollbarsOpacity(1);

  // An animation for the fade should be either null or cancelled, since
  // mouse is still near the scrollbar.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Scrollbar should still be thick and visible.
  time += kFadeDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Tests that main thread scroll updates immediately queue a fade out animation
TEST_P(ScrollbarAnimationControllerOverlayTest, MainThreadScrollQueuesFade) {
  ASSERT_TRUE(client_.start_fade().is_null());

  // A ScrollUpdate indicates a main thread scroll update so we should schedule
  // a fade out animation since there is no scroll end notification.
  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kFadeDelay, client_.delay());
}

// Tests that the fade effect is animated.
TEST_P(ScrollbarAnimationControllerOverlayTest, FadeAnimated) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollUpdate();

  // Appearance is instant.
  ExpectScrollbarsOpacity(1);

  // An fade out animation should have been enqueued.
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  std::move(client_.start_fade()).Run();

  // Test that at half the fade duration time, the opacity is at half.
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);

  time += kFadeDuration / 2;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(.5f);

  time += kFadeDuration / 2;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(0);
}

// Tests that the controller tells the client when the scrollbars hide/show.
TEST_P(ScrollbarAnimationControllerOverlayTest, NotifyChangedVisibility) {
  base::TimeTicks time;
  time += base::Seconds(1);

  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(1);
  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  Mock::VerifyAndClearExpectations(&client_);

  // Play out the fade out animation. We shouldn't notify that the scrollbars
  // are hidden until the animation is completly over. We can (but don't have
  // to) notify during the animation that the scrollbars are still visible.
  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(0);
  ASSERT_FALSE(client_.start_fade().is_null());
  std::move(client_.start_fade()).Run();
  scrollbar_controller_->Animate(time);
  time += kFadeDuration / 4;
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  scrollbar_controller_->Animate(time);
  time += kFadeDuration / 4;
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  scrollbar_controller_->Animate(time);
  time += kFadeDuration / 4;
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(.25f);
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(1);
  time += kFadeDuration / 4;
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
  ExpectScrollbarsOpacity(0);
  Mock::VerifyAndClearExpectations(&client_);

  // Calling DidScrollUpdate without a begin (i.e. update from commit) should
  // also notify.
  EXPECT_CALL(client_, DidChangeScrollbarVisibility()).Times(1);
  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  Mock::VerifyAndClearExpectations(&client_);
}

// Move the pointer near each scrollbar. Confirm it gets thick and narrow when
// moved away.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, MouseNearEach) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollUpdate();

  // Near vertical scrollbar.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(-1, 0));
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Subsequent moves within the nearness threshold should not change anything.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(-2, 0));
  scrollbar_controller_->Animate(time);
  time += base::Seconds(10);
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Now move away from bar.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(
      -mouse_move_distance_to_trigger_expand_ - 1, 0));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Near horizontal scrollbar
  scrollbar_controller_->DidMouseMove(NearHorizontalScrollbarBegin(0, -1));
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Subsequent moves within the nearness threshold should not change anything.
  scrollbar_controller_->DidMouseMove(NearHorizontalScrollbarBegin(0, -2));
  scrollbar_controller_->Animate(time);
  time += base::Seconds(10);
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Now move away from bar.
  scrollbar_controller_->DidMouseMove(NearHorizontalScrollbarBegin(
      0, -mouse_move_distance_to_trigger_expand_ - 1));
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // An fade out animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kFadeDelay, client_.delay());
}

// Move mouse near both scrollbars at the same time.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, MouseNearBoth) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollUpdate();

  // Move scrollbar thumb to the end of track.
  v_scrollbar_layer_->SetCurrentPos(100);
  h_scrollbar_layer_->SetCurrentPos(100);

  // Near both Scrollbar
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarEnd(-1, -1));
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(1, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Move mouse on top of the vertical scrollbar, then to the horizontal scrollbar
// before the fade out animation is finished. Then, move the pointer away from
// both scrollbars before animation the animation is finished.
TEST_P(ScrollbarAnimationControllerOverlayTest,
       MouseNearOtherBeforeAnimationFinished) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollUpdate();

  // Over vertical scrollbar.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(0, 0));
  scrollbar_controller_->Animate(time);

  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Vertical scrollbar animate to half thickened.
  time += kThinningDuration / 2;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale + (1.0f - kIdleThicknessScale) / 2,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Away from vertical scrollbar and over horizontal scrollbar.
  scrollbar_controller_->DidMouseMove(gfx::PointF(0, 0));
  scrollbar_controller_->DidMouseMove(NearHorizontalScrollbarBegin(0, 0));
  scrollbar_controller_->Animate(time);

  // Vertical scrollbar animates to thin. Horizontal scrollbar animates to
  // thickened.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(1, h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Away from horizontal scrollbar.
  scrollbar_controller_->DidMouseMove(gfx::PointF(0, 0));
  scrollbar_controller_->Animate(time);

  // Horizontal scrollbar animates to thin.
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(1);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // A fade out animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kFadeDelay, client_.delay());
}

// Ensure we have a delay fadeout animation after mouse leave without a mouse
// move.
TEST_P(ScrollbarAnimationControllerOverlayTest, MouseLeaveFadeOut) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Move mouse near scrollbar.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(0, 0));

  // Scroll to make the scrollbars visible.
  scrollbar_controller_->DidScrollUpdate();

  // Should not have delay fadeout animation.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Mouse leave.
  scrollbar_controller_->DidMouseLeave();

  // An fade out animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_EQ(kFadeDelay, client_.delay());
}

// Scrollbars should schedule a delay fade in when mouse hover the show
// scrollbar region of a hidden scrollbar.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest, BasicMouseHoverFadeIn) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Move mouse over the fade in region of scrollbar.
  scrollbar_controller_->DidMouseMove(
      NearVerticalScrollbarBegin(-mouse_move_distance_to_trigger_fade_in_, 0));

  // An fade in animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  EXPECT_EQ(kFadeDelay, client_.delay());

  // Play the delay animation.
  std::move(client_.start_fade()).Run();

  scrollbar_controller_->Animate(time);
  time += kFadeDuration / 2;
  scrollbar_controller_->Animate(time);

  ExpectScrollbarsOpacity(0.5);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());

  time += kFadeDuration / 2;
  scrollbar_controller_->Animate(time);

  ExpectScrollbarsOpacity(1);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
}

// Scrollbars should not schedule a new delay fade in when the mouse hovers
// inside a scrollbar already scheduled a delay fade in.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest,
       MouseHoverScrollbarAndMoveInside) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Move mouse over the fade in region of scrollbar.
  scrollbar_controller_->DidMouseMove(
      NearVerticalScrollbarBegin(-mouse_move_distance_to_trigger_fade_in_, 0));

  // A fade in animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  EXPECT_EQ(kFadeDelay, client_.delay());

  client_.start_fade().Reset();
  // Move mouse still hover the fade in region of scrollbar should not
  // post a new fade in.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(
      -mouse_move_distance_to_trigger_fade_in_ + 2, 0));

  EXPECT_TRUE(client_.start_fade().is_null());
}

// Scrollbars should cancel delay fade in when mouse hover hidden scrollbar then
// move far away.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest,
       MouseHoverThenOutShouldCancelFadeIn) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Move mouse over the fade in region of scrollbar.
  scrollbar_controller_->DidMouseMove(
      NearVerticalScrollbarBegin(-mouse_move_distance_to_trigger_fade_in_, 0));

  // An fade in animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  EXPECT_EQ(kFadeDelay, client_.delay());

  // Move mouse far away，delay fade in should be canceled.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(
      -mouse_move_distance_to_trigger_fade_in_ - 1, 0));

  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Scrollbars should cancel delay fade in when mouse hover hidden scrollbar then
// move out of window.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest,
       MouseHoverThenLeaveShouldCancelShowThenEnterShouldFadeIn) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Move mouse over the fade in region of scrollbar.
  scrollbar_controller_->DidMouseMove(
      NearVerticalScrollbarBegin(-mouse_move_distance_to_trigger_fade_in_, 0));

  // An fade in animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  EXPECT_EQ(kFadeDelay, client_.delay());

  // Move mouse out of window，delay fade in should be canceled.
  scrollbar_controller_->DidMouseLeave();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Move mouse over the fade in region of scrollbar.
  scrollbar_controller_->DidMouseMove(
      NearVerticalScrollbarBegin(-mouse_move_distance_to_trigger_fade_in_, 0));

  // An fade in animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  EXPECT_EQ(kFadeDelay, client_.delay());

  // Play the delay animation.
  std::move(client_.start_fade()).Run();

  scrollbar_controller_->Animate(time);
  time += kFadeDuration;
  scrollbar_controller_->Animate(time);

  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
}

// Make sure mouse down will cancel hover fade in timer, then mouse move with
// press will not trigger hover fade in, mouse release near will trigger new
// hover fade in.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest,
       MouseHoverThenMouseDownShouldCancelFadeInThenReleaseNearShouldFadeIn) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Move mouse over the fade in region of scrollbar.
  scrollbar_controller_->DidMouseMove(
      NearVerticalScrollbarBegin(-mouse_move_distance_to_trigger_fade_in_, 0));

  // An fade in animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  EXPECT_EQ(kFadeDelay, client_.delay());

  // Mouse down，delay fade in should be canceled.
  scrollbar_controller_->DidMouseDown();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Move mouse hover the fade in region of scrollbar with press.
  scrollbar_controller_->DidMouseMove(
      NearVerticalScrollbarBegin(-mouse_move_distance_to_trigger_fade_in_, 0));

  // Should not have delay fade animation.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Mouse up.
  scrollbar_controller_->DidMouseUp();

  // An fade in animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  EXPECT_EQ(kFadeDelay, client_.delay());

  // Play the delay animation.
  std::move(client_.start_fade()).Run();

  scrollbar_controller_->Animate(time);
  time += kFadeDuration;
  scrollbar_controller_->Animate(time);

  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
}

// Make sure mouse down will cancel hover fade in timer, then mouse move with
// press will not trigger hover fade in, mouse release far will not trigger new
// hover fade in.
TEST_F(ScrollbarAnimationControllerAuraOverlayTest,
       MouseReleaseFarShouldNotFadeIn) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Move mouse over the fade in region of scrollbar.
  scrollbar_controller_->DidMouseMove(
      NearVerticalScrollbarBegin(-mouse_move_distance_to_trigger_fade_in_, 0));

  // An fade in animation should have been enqueued.
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  EXPECT_EQ(kFadeDelay, client_.delay());

  // Mouse down，delay fade in should be canceled.
  scrollbar_controller_->DidMouseDown();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Move mouse far from hover the fade in region of scrollbar with
  // press.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(
      -mouse_move_distance_to_trigger_fade_in_ - 1, 0));

  // Should not have delay fade animation.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Mouse up.
  scrollbar_controller_->DidMouseUp();

  // Should not have delay fade animation.
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
}

// Ensure overlay scrollbars show and don't fade out when tickmarks are visible,
// and fade out when tickmarks hide.
TEST_P(ScrollbarAnimationControllerOverlayTest, TickmarksShowHide) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Overlay Scrollbar hidden at beginnging.
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Scrollbars show when tickmarks show.
  scrollbar_controller_->UpdateTickmarksVisibility(true);
  EXPECT_FALSE(scrollbar_controller_->ScrollbarsHidden());
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Scroll update, no delay fade animation.
  scrollbar_controller_->DidScrollUpdate();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Scroll update with phase, no delay fade animation.
  scrollbar_controller_->DidScrollUpdate();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Move mouse, no delay fade animation.
  scrollbar_controller_->DidMouseMove(NearVerticalScrollbarBegin(0, 0));
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Mouse leave, no delay fade animation.
  scrollbar_controller_->DidMouseLeave();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Scrollbars fade out animation has enqueued when tickmarks hide.
  scrollbar_controller_->UpdateTickmarksVisibility(false);
  EXPECT_FALSE(client_.start_fade().is_null());
  EXPECT_FALSE(client_.start_fade().IsCancelled());
  EXPECT_EQ(kFadeDelay, client_.delay());
}

// Test that Fluent scrollbar animations get properly queued when moving the
// mouse while the scrollbar is visible.
TEST_F(ScrollbarAnimationControllerFluentOverlayTest,
       DidMouseMoveWithVisibleScrollbarQueuesAnimations) {
  base::TimeTicks time;
  time += base::Seconds(1);
  ExpectScrollbarsOpacity(0.f);

  // Emulate scroll. Scrollbar should be fully shown.
  scrollbar_controller_->DidScrollUpdate();
  ExpectScrollbarsOpacity(1.f);

  // Process a MouseMove far from the scrollbar, a fade out animation should be
  // queued.
  scrollbar_controller_->DidMouseMove(gfx::PointF(50, 50));
  EXPECT_FALSE(client_.start_fade().is_null());
  std::move(client_.start_fade()).Run();
  scrollbar_controller_->Animate(time);
  time += kFadeDuration / 2.f;
  scrollbar_controller_->Animate(time);
  ExpectScrollbarsOpacity(0.5f);

  // Process a MouseMove on top the vertical scrollbar, then tick animations for
  // the thickness to update and check that it did.
  scrollbar_controller_->DidMouseMove(gfx::PointF(90, 10));
  scrollbar_controller_->Animate(time + kFadeDuration);
  ExpectScrollbarsOpacity(1.f);
}

// Ensure Fluent overlay scrollbar remains hidden when invisible and being
// moused over.
TEST_F(ScrollbarAnimationControllerFluentOverlayTest,
       DidMouseMoveWithHiddenScrollbarRemainsHidden) {
  ExpectScrollbarsOpacity(0.f);

  // Have the controller process a MouseMove on top of the vertical scrollbar
  // and check that no animation was queued.
  scrollbar_controller_->DidMouseMove(gfx::PointF(90, 10));
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
  ExpectScrollbarsOpacity(0.f);
}

// Ensure Fluent overlay scrollbar gets shown when scrolling, goes to Full Mode
// when mousing over it and disappears when the mouse moves away.
TEST_F(ScrollbarAnimationControllerFluentOverlayTest,
       ScrollAndMouseOverGoesToFullMode) {
  ExpectScrollbarsOpacity(0.f);

  // Emulate scroll. Scrollbar should be fully shown.
  scrollbar_controller_->DidScrollUpdate();
  ExpectScrollbarsOpacity(1.f);

  // Process a MouseMove on top the vertical scrollbar, then tick animations for
  // the scrollbar's thickness to update and check that it did.
  scrollbar_controller_->DidMouseMove(gfx::PointF(90, 10));
  base::TimeTicks time;
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  time += kThinningDuration;
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.f, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Process a MouseMove away from the vertical scrollbar, then check that a
  // fade out animation started.
  scrollbar_controller_->DidMouseMove(gfx::PointF(10, 10));
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  std::move(client_.start_fade()).Run();

  // Tick the animation and verify that the scrollbar disappears and its
  // thickness goes back to idle.
  scrollbar_controller_->Animate(time);
  scrollbar_controller_->Animate(time + kThinningDuration + kFadeDuration);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
  ExpectScrollbarsOpacity(0.f);
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Move the pointer out of the scroll area after scrolling and expect the
// scrollbar to fade.
TEST_F(ScrollbarAnimationControllerFluentOverlayTest, FadeWhenMouseLeaves) {
  ExpectScrollbarsOpacity(0.f);

  // Emulate scroll. Scrollbar should be fully shown.
  scrollbar_controller_->DidScrollUpdate();
  ExpectScrollbarsOpacity(1.f);

  // Emulate a MouseLeave and expect the scrollbar to eventually fade out.
  scrollbar_controller_->DidMouseLeave();
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  std::move(client_.start_fade()).Run();

  // Tick the animation and verify that the scrollbars disappear and its
  // thickness goes back to idle.
  base::TimeTicks time;
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  scrollbar_controller_->Animate(time + kThinningDuration + kFadeDuration);
  ExpectScrollbarsOpacity(0.f);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());
}

// Test that scrollbars disappear when it is captured and then is released after
// the mouse leaves the scrollable area.
TEST_F(ScrollbarAnimationControllerFluentOverlayTest,
       HidesWhenReleasedOutsideArea) {
  ExpectScrollbarsOpacity(0.f);

  // Emulate scroll. Scrollbar should be fully shown.
  scrollbar_controller_->DidScrollUpdate();
  ExpectScrollbarsOpacity(1.f);

  // Process a MouseMove on top the vertical scrollbar and capture it. It should
  // render at full thickness immediately (without needing to tick animations).
  scrollbar_controller_->DidMouseMove(gfx::PointF(90, 10));
  scrollbar_controller_->DidMouseDown();
  EXPECT_FLOAT_EQ(1.f, v_scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FLOAT_EQ(kIdleThicknessScale,
                  h_scrollbar_layer_->thumb_thickness_scale_factor());

  // Leave the scrollable area and verify that no animation is queued.
  scrollbar_controller_->DidMouseLeave();
  EXPECT_TRUE(client_.start_fade().is_null() ||
              client_.start_fade().IsCancelled());

  // Release the scrollbar, then check that a fade out animation started.
  scrollbar_controller_->DidMouseUp();
  EXPECT_EQ(kFadeDelay, client_.delay());
  EXPECT_FALSE(client_.start_fade().is_null());
  std::move(client_.start_fade()).Run();

  // Tick the animation and verify that the scrollbars disappear.
  base::TimeTicks time;
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  scrollbar_controller_->Animate(time + kFadeDuration);
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());
  ExpectScrollbarsOpacity(0.f);
}

class ScrollbarAnimationControllerAndroidTest
    : public LayerTreeImplTestBase,
      public testing::Test,
      public ScrollbarAnimationControllerClient {
 public:
  ScrollbarAnimationControllerAndroidTest()
      : did_request_redraw_(false), did_request_animate_(false) {}

  void PostDelayedScrollbarAnimationTask(base::OnceClosure start_fade,
                                         base::TimeDelta delay) override {
    start_fade_ = std::move(start_fade);
    delay_ = delay;
  }
  void SetNeedsRedrawForScrollbarAnimation() override {
    did_request_redraw_ = true;
  }
  void SetNeedsAnimateForScrollbarAnimation() override {
    did_request_animate_ = true;
  }
  ScrollbarSet ScrollbarsFor(ElementId scroll_element_id) const override {
    return host_impl()->ScrollbarsFor(scroll_element_id);
  }
  void DidChangeScrollbarVisibility() override {}
  bool IsFluentOverlayScrollbar() const override { return false; }

  void UpdateScrollbarGeometriesAndDrawProperties() {
    host_impl()->active_tree()->UpdateAllScrollbarGeometriesForTesting();
    UpdateActiveTreeDrawProperties();
  }

 protected:
  void SetUp() override {
    const int kTrackStart = 0;
    const bool kIsLeftSideVerticalScrollbar = false;

    LayerImpl* root = root_layer();
    scroll_layer_ = AddLayerInActiveTree<LayerImpl>();
    scrollbar_layer_ = AddLayerInActiveTree<SolidColorScrollbarLayerImpl>(
        orientation(), kThumbThickness, kTrackStart,
        kIsLeftSideVerticalScrollbar);
    SetElementIdsForTesting();

    scroll_layer_->SetBounds(gfx::Size(200, 200));
    CopyProperties(root, scroll_layer_);
    CreateTransformNode(scroll_layer_);
    CreateScrollNode(scroll_layer_, gfx::Size(100, 100));

    scrollbar_layer_->SetScrollElementId(scroll_layer_->element_id());
    CopyProperties(scroll_layer_, scrollbar_layer_);
    auto& scrollbar_effect = CreateEffectNode(scrollbar_layer_);
    scrollbar_effect.opacity = 0.f;
    scrollbar_effect.has_potential_opacity_animation = true;

    UpdateScrollbarGeometriesAndDrawProperties();

    scrollbar_controller_ =
        ScrollbarAnimationController::CreateScrollbarAnimationControllerAndroid(
            scroll_layer_->element_id(), this, base::Seconds(2),
            base::Seconds(3), 0.0f);
  }

  virtual ScrollbarOrientation orientation() const {
    return ScrollbarOrientation::kHorizontal;
  }

  std::unique_ptr<ScrollbarAnimationController> scrollbar_controller_;
  raw_ptr<LayerImpl> scroll_layer_;
  raw_ptr<SolidColorScrollbarLayerImpl> scrollbar_layer_;

  base::OnceClosure start_fade_;
  base::TimeDelta delay_;
  bool did_request_redraw_;
  bool did_request_animate_;
};

class VerticalScrollbarAnimationControllerAndroidTest
    : public ScrollbarAnimationControllerAndroidTest {
 protected:
  ScrollbarOrientation orientation() const override {
    return ScrollbarOrientation::kVertical;
  }
};

TEST_F(ScrollbarAnimationControllerAndroidTest, HiddenInBegin) {
  scrollbar_layer_->SetOverlayScrollbarLayerOpacityAnimated(
      0.f, /*fade_out_animation=*/false);
  scrollbar_controller_->Animate(base::TimeTicks());
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
}

TEST_F(ScrollbarAnimationControllerAndroidTest,
       HiddenAfterNonScrollingGesture) {
  scrollbar_layer_->SetOverlayScrollbarLayerOpacityAnimated(
      0.f, /*fade_out_animation=*/false);

  base::TimeTicks time;
  time += base::Seconds(100);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());

  EXPECT_TRUE(start_fade_.is_null());

  time += base::Seconds(100);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
}

// Confirm the scrollbar does not appear on WillUpdateScroll on Android.
TEST_F(ScrollbarAnimationControllerAndroidTest,
       WillUpdateScrollNotAppearScrollbar) {
  base::TimeTicks time;
  time += base::Seconds(1);

  // Scrollbar should be invisible.
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());

  // Scrollbar should appear when scroll will update.
  scrollbar_controller_->WillUpdateScroll();
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
  EXPECT_TRUE(scrollbar_controller_->ScrollbarsHidden());

  // No fade out animation should have been enqueued.
  EXPECT_TRUE(start_fade_.is_null());
}

TEST_F(ScrollbarAnimationControllerAndroidTest, HideOnResize) {
  EXPECT_EQ(gfx::Size(200, 200), scroll_layer_->bounds());

  EXPECT_EQ(ScrollbarOrientation::kHorizontal, scrollbar_layer_->orientation());

  // Shrink along X axis, horizontal scrollbar should appear.
  GetScrollNode(scroll_layer_.get())->container_bounds = gfx::Size(100, 200);
  UpdateScrollbarGeometriesAndDrawProperties();

  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  // Shrink along Y axis and expand along X, horizontal scrollbar
  // should disappear.
  GetScrollNode(scroll_layer_.get())->container_bounds = gfx::Size(200, 100);
  UpdateScrollbarGeometriesAndDrawProperties();

  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
}

TEST_F(VerticalScrollbarAnimationControllerAndroidTest, HideOnResize) {
  EXPECT_EQ(gfx::Size(200, 200), scroll_layer_->bounds());

  EXPECT_EQ(ScrollbarOrientation::kVertical, scrollbar_layer_->orientation());

  // Shrink along X axis, vertical scrollbar should remain invisible.
  GetScrollNode(scroll_layer_.get())->container_bounds = gfx::Size(100, 200);
  UpdateScrollbarGeometriesAndDrawProperties();

  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());

  // Shrink along Y axis and expand along X, vertical scrollbar should appear.
  GetScrollNode(scroll_layer_.get())->container_bounds = gfx::Size(200, 100);
  UpdateScrollbarGeometriesAndDrawProperties();

  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
}

TEST_F(ScrollbarAnimationControllerAndroidTest, HideOnUserNonScrollableHorz) {
  EXPECT_EQ(ScrollbarOrientation::kHorizontal, scrollbar_layer_->orientation());

  GetScrollNode(scroll_layer_.get())->user_scrollable_horizontal = false;
  UpdateScrollbarGeometriesAndDrawProperties();

  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
}

TEST_F(ScrollbarAnimationControllerAndroidTest, ShowOnUserNonScrollableVert) {
  EXPECT_EQ(ScrollbarOrientation::kHorizontal, scrollbar_layer_->orientation());

  GetScrollNode(scroll_layer_.get())->user_scrollable_vertical = false;
  UpdateScrollbarGeometriesAndDrawProperties();

  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
}

TEST_F(VerticalScrollbarAnimationControllerAndroidTest,
       HideOnUserNonScrollableVert) {
  EXPECT_EQ(ScrollbarOrientation::kVertical, scrollbar_layer_->orientation());

  GetScrollNode(scroll_layer_.get())->user_scrollable_vertical = false;
  UpdateScrollbarGeometriesAndDrawProperties();

  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
}

TEST_F(VerticalScrollbarAnimationControllerAndroidTest,
       ShowOnUserNonScrollableHorz) {
  EXPECT_EQ(ScrollbarOrientation::kVertical, scrollbar_layer_->orientation());

  GetScrollNode(scroll_layer_.get())->user_scrollable_horizontal = false;
  UpdateScrollbarGeometriesAndDrawProperties();

  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
}

TEST_F(ScrollbarAnimationControllerAndroidTest, AwakenByScrollingGesture) {
  base::TimeTicks time;
  time += base::Seconds(1);
  EXPECT_FALSE(did_request_animate_);

  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  EXPECT_FALSE(start_fade_.is_null());

  time += base::Seconds(100);

  scrollbar_controller_->Animate(time);
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());
  EXPECT_FALSE(did_request_animate_);
  std::move(start_fade_).Run();
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;

  time += base::Seconds(2);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);

  scrollbar_controller_->DidScrollUpdate();

  std::move(start_fade_).Run();
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;

  time += base::Seconds(2);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
}

TEST_F(ScrollbarAnimationControllerAndroidTest, AwakenByProgrammaticScroll) {
  base::TimeTicks time;
  time += base::Seconds(1);
  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FALSE(did_request_animate_);

  std::move(start_fade_).Run();
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());
  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FALSE(did_request_animate_);

  std::move(start_fade_).Run();
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  time += base::Seconds(2);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->DidScrollUpdate();
  std::move(start_fade_).Run();
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
}

TEST_F(ScrollbarAnimationControllerAndroidTest,
       AnimationPreservedByNonScrollingGesture) {
  base::TimeTicks time;
  time += base::Seconds(1);
  scrollbar_controller_->DidScrollUpdate();
  std::move(start_fade_).Run();
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  scrollbar_layer_->ResetChangeTracking();
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());
  EXPECT_EQ(scrollbar_layer_->GetDamageReasons(),
            DamageReasonSet{DamageReason::kScrollbarFadeOutAnimation});

  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());

  scrollbar_layer_->ResetChangeTracking();
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->Opacity());
  EXPECT_EQ(scrollbar_layer_->GetDamageReasons(),
            DamageReasonSet{DamageReason::kScrollbarFadeOutAnimation});

  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->Opacity());

  scrollbar_layer_->ResetChangeTracking();
  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->Opacity());
  EXPECT_EQ(scrollbar_layer_->GetDamageReasons(),
            DamageReasonSet{DamageReason::kScrollbarFadeOutAnimation});
}

TEST_F(ScrollbarAnimationControllerAndroidTest,
       AnimationOverriddenByScrollingGesture) {
  base::TimeTicks time;
  time += base::Seconds(1);
  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FALSE(did_request_animate_);
  std::move(start_fade_).Run();
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_TRUE(did_request_animate_);
  did_request_animate_ = false;
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  scrollbar_controller_->DidScrollUpdate();
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(1, scrollbar_layer_->Opacity());

  time += base::Seconds(1);
  EXPECT_FALSE(did_request_animate_);
  EXPECT_FLOAT_EQ(1, scrollbar_layer_->Opacity());
}

INSTANTIATE_TEST_SUITE_P(All, ScrollbarAnimationControllerOverlayTest, Bool());
}  // namespace

}  // namespace cc
