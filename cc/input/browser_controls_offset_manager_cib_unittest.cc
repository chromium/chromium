// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_offset_manager_cib.h"

#include "base/time/time.h"
#include "cc/input/browser_controls_animation_params.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/input/browser_controls_offset_tags.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "components/viz/common/quads/offset_tag.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
namespace {

using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::NiceMock;

class MockClient : public BrowserControlsOffsetManagerClient {
 public:
  float TopControlsHeight() const override { return top_controls_height_; }
  float TopControlsMinHeight() const override { return 0.0f; }
  float BottomControlsHeight() const override {
    return bottom_controls_height_;
  }
  float BottomControlsMinHeight() const override { return 0.0f; }
  // Mirrors LayerTreeHostImpl, which reports a position change whenever a
  // ratio changes.
  void SetCurrentBrowserControlsShownRatio(float top_ratio,
                                           float bottom_ratio) override {
    if (top_ratio == top_ratio_ && bottom_ratio == bottom_ratio_) {
      return;
    }
    top_ratio_ = top_ratio;
    bottom_ratio_ = bottom_ratio;
    DidChangeBrowserControlsPosition();
  }
  float CurrentTopControlsShownRatio() const override { return top_ratio_; }
  float CurrentBottomControlsShownRatio() const override {
    return bottom_ratio_;
  }
  gfx::PointF ViewportScrollOffset() const override { return gfx::PointF(); }
  float MaxViewportScrollOffsetY() const override { return 1000.0f; }
  MOCK_METHOD(void, DidChangeBrowserControlsPosition, (), (override));
  bool OnlyExpandTopControlsAtPageTop() const override { return false; }
  bool HaveRootScrollNode() const override { return true; }
  MOCK_METHOD(void, SetNeedsCommit, (BeginMainFrameReason), (override));
  float DeviceScaleFactor() const override { return 1.0f; }

  void set_heights(float top, float bottom) {
    top_controls_height_ = top;
    bottom_controls_height_ = bottom;
  }

 private:
  float top_controls_height_ = 100.0f;
  float bottom_controls_height_ = 50.0f;
  float top_ratio_ = 1.0f;
  float bottom_ratio_ = 1.0f;
};

class BrowserControlsOffsetManagerCibTest : public ::testing::Test {
 protected:
  NiceMock<MockClient> client_;
  BrowserControlsOffsetManagerCib manager_{&client_};
};

TEST_F(BrowserControlsOffsetManagerCibTest,
       GetOffsetTagValuesReturnsOnlyContentOffsetTag) {
  EXPECT_THAT(manager_.GetOffsetTagValues(), IsEmpty());

  const viz::OffsetTag content_tag = viz::OffsetTag::CreateRandom();
  BrowserControlsOffsetTagModifications modifications;
  modifications.tags.top_controls_offset_tag = viz::OffsetTag::CreateRandom();
  modifications.tags.content_offset_tag = content_tag;
  modifications.tags.bottom_controls_offset_tag =
      viz::OffsetTag::CreateRandom();
  modifications.top_controls_additional_height = 5;
  modifications.bottom_controls_additional_height = 10;
  manager_.SetOffsetTagModifications(modifications);

  EXPECT_THAT(manager_.GetOffsetTagValues(),
              ElementsAre(FieldsAre(content_tag, gfx::Vector2dF(0.0f, 0.0f))));

  client_.SetCurrentBrowserControlsShownRatio(0.256f, 0.5f);
  EXPECT_THAT(
      manager_.GetOffsetTagValues(),
      ElementsAre(FieldsAre(content_tag, gfx::Vector2dF(0.0f, -74.0f))));

  client_.SetCurrentBrowserControlsShownRatio(0.0f, 0.0f);
  EXPECT_THAT(
      manager_.GetOffsetTagValues(),
      ElementsAre(FieldsAre(content_tag, gfx::Vector2dF(0.0f, -100.0f))));
}

TEST_F(BrowserControlsOffsetManagerCibTest, ScrollIsNotConsumed) {
  EXPECT_CALL(client_, DidChangeBrowserControlsPosition()).Times(0);
  constexpr gfx::Vector2dF kScrollDelta(0.0f, 30.0f);
  manager_.ScrollBegin();
  EXPECT_EQ(kScrollDelta,
            manager_.ScrollBy(kScrollDelta, /*is_inertial=*/false));
  EXPECT_EQ(kScrollDelta,
            manager_.ScrollBy(kScrollDelta, /*is_inertial=*/true));
  manager_.ScrollEnd(gfx::Vector2dF());
  EXPECT_FLOAT_EQ(1.0f, manager_.TopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.0f, manager_.BottomControlsShownRatio());
  EXPECT_FALSE(manager_.HasAnimation());
  EXPECT_EQ(gfx::Vector2dF(), manager_.Animate(base::TimeTicks()));
}

constexpr base::TimeTicks kStartTime =
    base::TimeTicks() + base::Milliseconds(1000);

// Defaults to a linear curve so that expected values are exact.
BrowserControlsAnimationParams AnimationParams(float top,
                                               float bottom,
                                               base::TimeDelta duration) {
  return BrowserControlsAnimationParams{
      .target_top_controls_visible_height = top,
      .target_bottom_controls_visible_height = bottom,
      .duration = duration,
  };
}

// A zero-duration animation applies `top` and `bottom` on the next frame.
BrowserControlsAnimationParams InstantParams(float top, float bottom) {
  return AnimationParams(top, bottom, base::TimeDelta());
}

TEST_F(BrowserControlsOffsetManagerCibTest,
       ZeroDurationAnimationIsAppliedOnNextAnimateWithCounterScroll) {
  // SetAnimation() only requests a frame; the ratios change together with the
  // counter-scroll on the next Animate(), not before.
  EXPECT_CALL(client_, DidChangeBrowserControlsPosition());
  manager_.SetAnimation(InstantParams(25.0f, 10.0f));
  EXPECT_FLOAT_EQ(1.0f, manager_.TopControlsShownRatio());
  EXPECT_TRUE(manager_.HasAnimation());
  ::testing::Mock::VerifyAndClearExpectations(&client_);

  // Hiding 75px of the top controls moves the content up by 75px; the
  // compositor counter-scrolls by the same amount in the same frame.
  EXPECT_EQ(gfx::Vector2dF(0, -75), manager_.Animate(base::TimeTicks()));
  EXPECT_FLOAT_EQ(0.25f, manager_.TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.2f, manager_.BottomControlsShownRatio());
  EXPECT_FLOAT_EQ(25.0f, manager_.ContentTopOffset());
  EXPECT_FLOAT_EQ(10.0f, manager_.ContentBottomOffset());
  EXPECT_FALSE(manager_.HasAnimation());

  EXPECT_EQ(gfx::Vector2dF(), manager_.Animate(base::TimeTicks()));

  // Only the top controls counter-scroll the page.
  manager_.SetAnimation(InstantParams(25.0f, 50.0f));
  EXPECT_TRUE(manager_.HasAnimation());

  EXPECT_EQ(gfx::Vector2dF(), manager_.Animate(base::TimeTicks()));
  EXPECT_FLOAT_EQ(0.25f, manager_.TopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.0f, manager_.BottomControlsShownRatio());
  EXPECT_FALSE(manager_.HasAnimation());
}

TEST_F(BrowserControlsOffsetManagerCibTest, LaterAnimationOverridesEarlier) {
  manager_.SetAnimation(AnimationParams(60.0f, 50.0f, base::Milliseconds(200)));
  manager_.SetAnimation(AnimationParams(40.0f, 50.0f, base::Milliseconds(200)));
  EXPECT_TRUE(manager_.HasAnimation());

  manager_.Animate(kStartTime);
  EXPECT_TRUE(manager_.HasAnimation());

  EXPECT_EQ(gfx::Vector2dF(0, -30),
            manager_.Animate(kStartTime + base::Milliseconds(100)));
  EXPECT_TRUE(manager_.HasAnimation());

  EXPECT_EQ(gfx::Vector2dF(0, -30),
            manager_.Animate(kStartTime + base::Milliseconds(200)));
  EXPECT_FLOAT_EQ(0.4f, manager_.TopControlsShownRatio());
  EXPECT_FALSE(manager_.HasAnimation());

  // Animating back to the current heights before Animate() cancels the pending
  // animation.
  manager_.SetAnimation(AnimationParams(80.0f, 20.0f, base::Milliseconds(200)));
  manager_.SetAnimation(AnimationParams(40.0f, 50.0f, base::Milliseconds(200)));
  EXPECT_FALSE(manager_.HasAnimation());

  EXPECT_EQ(gfx::Vector2dF(),
            manager_.Animate(kStartTime + base::Milliseconds(300)));
  EXPECT_FLOAT_EQ(0.4f, manager_.TopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.0f, manager_.BottomControlsShownRatio());
  EXPECT_FALSE(manager_.HasAnimation());
}

TEST_F(BrowserControlsOffsetManagerCibTest,
       SetAnimationToCurrentHeightsIsNoOp) {
  EXPECT_CALL(client_, DidChangeBrowserControlsPosition()).Times(0);
  manager_.SetAnimation(InstantParams(100.0f, 50.0f));
  EXPECT_FALSE(manager_.HasAnimation());
}

TEST_F(BrowserControlsOffsetManagerCibTest,
       SetAnimationClampsHeightsAndKeepsRatioForZeroHeight) {
  // Heights outside [0, controls height] clamp to the ratio range.
  manager_.SetAnimation(InstantParams(150.0f, -5.0f));
  manager_.Animate(base::TimeTicks());
  EXPECT_FLOAT_EQ(1.0f, manager_.TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager_.BottomControlsShownRatio());
  EXPECT_FALSE(manager_.HasAnimation());

  manager_.SetAnimation(InstantParams(25.0f, 50.0f));
  manager_.Animate(base::TimeTicks());
  ASSERT_FLOAT_EQ(0.25f, manager_.TopControlsShownRatio());
  ASSERT_FLOAT_EQ(1.0f, manager_.BottomControlsShownRatio());
  EXPECT_FALSE(manager_.HasAnimation());

  // A control with zero height keeps its ratio instead of dividing by zero.
  client_.set_heights(/*top=*/0.0f, /*bottom=*/0.0f);
  manager_.SetAnimation(InstantParams(0.0f, 0.0f));
  EXPECT_FALSE(manager_.HasAnimation());
  EXPECT_EQ(gfx::Vector2dF(), manager_.Animate(base::TimeTicks()));
  EXPECT_FLOAT_EQ(0.25f, manager_.TopControlsShownRatio());
  EXPECT_FLOAT_EQ(1.0f, manager_.BottomControlsShownRatio());
}

TEST_F(BrowserControlsOffsetManagerCibTest,
       SetAnimationInterpolatesAndCounterScrolls) {
  EXPECT_CALL(client_, DidChangeBrowserControlsPosition());
  manager_.SetAnimation(AnimationParams(0.0f, 0.0f, base::Milliseconds(200)));
  EXPECT_TRUE(manager_.HasAnimation());
  ::testing::Mock::VerifyAndClearExpectations(&client_);

  // The first tick starts the animation at the current ratios.
  EXPECT_EQ(gfx::Vector2dF(), manager_.Animate(kStartTime));
  EXPECT_TRUE(manager_.HasAnimation());
  EXPECT_FLOAT_EQ(1.0f, manager_.TopControlsShownRatio());

  // Halfway: top 100px -> 50px, bottom 50px -> 25px. The page is
  // counter-scrolled by the 50px change in the top controls.
  EXPECT_EQ(gfx::Vector2dF(0, -50),
            manager_.Animate(kStartTime + base::Milliseconds(100)));
  EXPECT_FLOAT_EQ(0.5f, manager_.TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.5f, manager_.BottomControlsShownRatio());
  EXPECT_TRUE(manager_.HasAnimation());

  EXPECT_EQ(gfx::Vector2dF(0, -50),
            manager_.Animate(kStartTime + base::Milliseconds(200)));
  EXPECT_FLOAT_EQ(0.0f, manager_.TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager_.BottomControlsShownRatio());
  EXPECT_FALSE(manager_.HasAnimation());
}

TEST_F(BrowserControlsOffsetManagerCibTest, SetAnimationEasesWithCurve) {
  // ease-in: the midpoint lags behind linear progress.
  const gfx::CubicBezier ease_in(0.42, 0.0, 1.0, 1.0);
  BrowserControlsAnimationParams params =
      AnimationParams(0.0f, 50.0f, base::Milliseconds(200));
  params.curve = ease_in;
  manager_.SetAnimation(params);
  EXPECT_TRUE(manager_.HasAnimation());

  manager_.Animate(kStartTime);
  EXPECT_TRUE(manager_.HasAnimation());

  manager_.Animate(kStartTime + base::Milliseconds(100));
  EXPECT_FLOAT_EQ(1.0f - static_cast<float>(ease_in.Solve(0.5)),
                  manager_.TopControlsShownRatio());
  EXPECT_GT(manager_.TopControlsShownRatio(), 0.5f);
  EXPECT_TRUE(manager_.HasAnimation());
}

TEST_F(BrowserControlsOffsetManagerCibTest,
       BottomOnlyAnimationDoesNotCounterScroll) {
  manager_.SetAnimation(AnimationParams(100.0f, 0.0f, base::Milliseconds(200)));
  EXPECT_TRUE(manager_.HasAnimation());

  EXPECT_EQ(gfx::Vector2dF(), manager_.Animate(kStartTime));
  EXPECT_TRUE(manager_.HasAnimation());

  EXPECT_EQ(gfx::Vector2dF(),
            manager_.Animate(kStartTime + base::Milliseconds(100)));
  EXPECT_FLOAT_EQ(0.5f, manager_.BottomControlsShownRatio());
  EXPECT_TRUE(manager_.HasAnimation());

  EXPECT_EQ(gfx::Vector2dF(),
            manager_.Animate(kStartTime + base::Milliseconds(200)));
  EXPECT_FLOAT_EQ(1.0f, manager_.TopControlsShownRatio());
  EXPECT_FLOAT_EQ(0.0f, manager_.BottomControlsShownRatio());
  EXPECT_FALSE(manager_.HasAnimation());
}

TEST_F(BrowserControlsOffsetManagerCibTest,
       SetAnimationReplacesRunningAnimation) {
  manager_.SetAnimation(AnimationParams(0.0f, 0.0f, base::Milliseconds(200)));
  EXPECT_TRUE(manager_.HasAnimation());

  manager_.Animate(kStartTime);
  EXPECT_TRUE(manager_.HasAnimation());

  manager_.Animate(kStartTime + base::Milliseconds(100));
  ASSERT_FLOAT_EQ(0.5f, manager_.TopControlsShownRatio());
  EXPECT_TRUE(manager_.HasAnimation());

  // Animate back to fully shown from the current position.
  manager_.SetAnimation(
      AnimationParams(100.0f, 50.0f, base::Milliseconds(100)));
  EXPECT_TRUE(manager_.HasAnimation());

  const base::TimeTicks restart = kStartTime + base::Milliseconds(100);
  manager_.Animate(restart);
  EXPECT_TRUE(manager_.HasAnimation());

  EXPECT_EQ(gfx::Vector2dF(0, 25),
            manager_.Animate(restart + base::Milliseconds(50)));
  EXPECT_FLOAT_EQ(0.75f, manager_.TopControlsShownRatio());
  EXPECT_TRUE(manager_.HasAnimation());

  manager_.Animate(restart + base::Milliseconds(100));
  EXPECT_FLOAT_EQ(1.0f, manager_.TopControlsShownRatio());
  EXPECT_FALSE(manager_.HasAnimation());
}

TEST_F(BrowserControlsOffsetManagerCibTest, ResetAnimationsStopsInPlace) {
  manager_.SetAnimation(AnimationParams(0.0f, 0.0f, base::Milliseconds(200)));
  EXPECT_TRUE(manager_.HasAnimation());

  manager_.Animate(kStartTime);
  EXPECT_TRUE(manager_.HasAnimation());

  manager_.Animate(kStartTime + base::Milliseconds(100));
  EXPECT_TRUE(manager_.HasAnimation());

  manager_.ResetAnimations();
  EXPECT_FLOAT_EQ(0.5f, manager_.TopControlsShownRatio());
  EXPECT_FALSE(manager_.HasAnimation());

  EXPECT_EQ(gfx::Vector2dF(),
            manager_.Animate(kStartTime + base::Milliseconds(200)));
  EXPECT_FLOAT_EQ(0.5f, manager_.TopControlsShownRatio());
  EXPECT_FALSE(manager_.HasAnimation());
}

}  // namespace
}  // namespace cc
