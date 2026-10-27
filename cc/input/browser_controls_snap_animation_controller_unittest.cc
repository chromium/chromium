// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_snap_animation_controller.h"

#include <optional>

#include "base/test/scoped_mock_clock_override.h"
#include "base/time/time.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
namespace {

class FakeBrowserControlsOffsetManagerClient
    : public BrowserControlsOffsetManagerClient {
 public:
  // Default controls geometry for tests. The always-shown region height is
  // therefore 50 (half the top height), and the top can-hide region height
  // ranges from 150 to 300 (1.5x to 3x the top height).
  static constexpr float kTopControlsHeight = 100.0f;
  static constexpr float kBottomControlsHeight = 0.0f;
  static constexpr float kMaxViewportOffsetY = 2000.0f;
  static constexpr float kDeviceScaleFactor = 1.0f;
  static constexpr float kInitialViewportOffsetY = 500.0f;

  FakeBrowserControlsOffsetManagerClient() = default;
  ~FakeBrowserControlsOffsetManagerClient() override = default;

  // BrowserControlsOffsetManagerClient:
  float TopControlsHeight() const override { return top_controls_height_; }
  float TopControlsMinHeight() const override {
    return top_controls_min_height_;
  }
  float BottomControlsHeight() const override {
    return bottom_controls_height_;
  }
  float BottomControlsMinHeight() const override {
    return bottom_controls_min_height_;
  }
  void SetCurrentBrowserControlsShownRatio(float top_ratio,
                                           float bottom_ratio) override {}
  float CurrentTopControlsShownRatio() const override { return 1.0f; }
  float CurrentBottomControlsShownRatio() const override { return 1.0f; }
  gfx::PointF ViewportScrollOffset() const override { return viewport_offset_; }
  float MaxViewportScrollOffsetY() const override {
    return kMaxViewportOffsetY;
  }
  void DidChangeBrowserControlsPosition() override {}
  bool OnlyExpandTopControlsAtPageTop() const override { return false; }
  bool HaveRootScrollNode() const override { return true; }
  void SetNeedsCommit(BeginMainFrameReason reason) override {}
  float DeviceScaleFactor() const override { return kDeviceScaleFactor; }

  void set_top_controls_height(float height) { top_controls_height_ = height; }
  void set_top_controls_min_height(float min_height) {
    top_controls_min_height_ = min_height;
  }
  void set_bottom_controls_height(float height) {
    bottom_controls_height_ = height;
  }
  void set_viewport_offset_y(float y) { viewport_offset_.set_y(y); }

 private:
  float top_controls_height_ = kTopControlsHeight;
  float top_controls_min_height_ = 0.0f;
  float bottom_controls_height_ = kBottomControlsHeight;
  float bottom_controls_min_height_ = 0.0f;
  gfx::PointF viewport_offset_{0.0f, kInitialViewportOffsetY};
};

class BrowserControlsSnapAnimationControllerTest : public testing::Test {
 protected:
  using SnapHint = BrowserControlsSnapHint;
  static constexpr int64_t kShowHideMaxDurationMs = 200;

  BrowserControlsSnapAnimationControllerTest()
      : controller_(&client_, kShowHideMaxDurationMs) {}

  std::optional<SnapHint> SendTwoScrollUpdates(
      float total_delta_y,
      base::TimeDelta interval = base::Milliseconds(10),
      bool is_inertial = false) {
    float half = total_delta_y / 2.0f;
    // The first sample after OnScrollBegin() never triggers a hint because at
    // least two samples are required to compute scroll velocity.
    EXPECT_FALSE(
        controller_.OnScrollUpdate(gfx::Vector2dF(0.0f, half), is_inertial)
            .has_value());
    clock_.Advance(interval);
    return controller_.OnScrollUpdate(gfx::Vector2dF(0.0f, half), is_inertial);
  }

  base::ScopedMockClockOverride clock_;
  FakeBrowserControlsOffsetManagerClient client_;
  BrowserControlsSnapAnimationController controller_;
};

TEST_F(BrowserControlsSnapAnimationControllerTest,
       RequiresTwoSamplesBeforeTriggering) {
  controller_.OnScrollBegin();
  float threshold = controller_.TriggerThreshold(1.0f);

  // A single sample above threshold does not trigger.
  auto hint1 = controller_.OnScrollUpdate(gfx::Vector2dF(0.0f, threshold + 10),
                                          /*is_inertial=*/false);
  EXPECT_FALSE(hint1.has_value());

  // The second sample triggers.
  clock_.Advance(base::Milliseconds(10));
  auto hint2 = controller_.OnScrollUpdate(gfx::Vector2dF(0.0f, 1.0f),
                                          /*is_inertial=*/false);
  ASSERT_TRUE(hint2.has_value());
  EXPECT_EQ(hint2->action, SnapHint::Action::kCanHide);
  EXPECT_LT(hint2->slowness, 1.0f);
  controller_.OnScrollEnd();
}

TEST_F(BrowserControlsSnapAnimationControllerTest,
       OnScrollBeginResetsVelocityTracker) {
  controller_.OnScrollBegin();
  float threshold = controller_.TriggerThreshold(1.0f);

  // First sample in scroll sequence 1.
  auto hint1 = controller_.OnScrollUpdate(gfx::Vector2dF(0.0f, threshold + 10),
                                          /*is_inertial=*/false);
  EXPECT_FALSE(hint1.has_value());

  // Starting a new scroll without an intervening OnScrollEnd() still resets the
  // velocity tracker, so the first sample in scroll sequence 2 is sample #1 and
  // does not trigger.
  controller_.OnScrollBegin();
  clock_.Advance(base::Milliseconds(10));
  auto hint2 = controller_.OnScrollUpdate(gfx::Vector2dF(0.0f, threshold + 10),
                                          /*is_inertial=*/false);
  EXPECT_FALSE(hint2.has_value());
  controller_.OnScrollEnd();
}

TEST_F(BrowserControlsSnapAnimationControllerTest,
       DirectionReversalResetsVelocityTracker) {
  controller_.OnScrollBegin();
  float threshold = controller_.TriggerThreshold(1.0f);

  // First sample scrolling down.
  constexpr float kInitialDownwardDelta = 5.0f;
  controller_.OnScrollUpdate(gfx::Vector2dF(0.0f, kInitialDownwardDelta),
                             false);
  clock_.Advance(base::Milliseconds(10));

  // Reverse direction (up): this resets the velocity tracker, so this single
  // upward sample is only sample #1 in the new direction and must not trigger,
  // even when its magnitude overcomes `kInitialDownwardDelta` and exceeds
  // `threshold` in the upward direction.
  auto hint = controller_.OnScrollUpdate(
      gfx::Vector2dF(0.0f, -(threshold + kInitialDownwardDelta + 1.0f)), false);
  EXPECT_FALSE(hint.has_value());

  // A second upward sample now triggers.
  clock_.Advance(base::Milliseconds(10));
  auto hint2 = controller_.OnScrollUpdate(gfx::Vector2dF(0.0f, -1.0f), false);
  ASSERT_TRUE(hint2.has_value());
  EXPECT_EQ(hint2->action, SnapHint::Action::kCanShow);
  controller_.OnScrollEnd();
}

TEST_F(BrowserControlsSnapAnimationControllerTest,
       MustShowImmediatelyInAlwaysShownRegion) {
  // Place viewport inside always-shown region (<= 50.0 for height 100).
  client_.set_viewport_offset_y(40.0f);
  EXPECT_TRUE(controller_.ViewportInAlwaysShownRegion());

  // A zero-velocity OnScrollEnd() or downward scroll in the always-shown region
  // must not trigger a show animation.
  controller_.OnScrollBegin();
  EXPECT_FALSE(controller_.OnScrollEnd().has_value());

  controller_.OnScrollBegin();
  EXPECT_FALSE(SendTwoScrollUpdates(10.0f, base::Milliseconds(50)).has_value());
  EXPECT_FALSE(controller_.OnScrollEnd().has_value());

  // Even a tiny upward scroll delta well below the trigger threshold emits
  // kMustShow immediately when the pre-scroll offset is just outside the
  // always-shown region and the in-flight delta crosses into it.
  controller_.OnScrollBegin();
  client_.set_viewport_offset_y(50.25f);
  EXPECT_FALSE(controller_.ViewportInAlwaysShownRegion());
  auto hint = SendTwoScrollUpdates(-1.0f, base::Milliseconds(50));
  ASSERT_TRUE(hint.has_value());
  EXPECT_EQ(hint->action, SnapHint::Action::kMustShow);
  controller_.OnScrollEnd();
}

TEST_F(BrowserControlsSnapAnimationControllerTest,
       InertialScrollUsesMinimumCanHideRegion) {
  // Position viewport between min can-hide region (150) and max can-hide region
  // (300).
  client_.set_viewport_offset_y(200.0f);
  float threshold = controller_.TriggerThreshold(1.0f);

  // Non-inertial slow scroll (slowness ~ 1.0) requires viewport_offset_y >=
  // 300, so it does not trigger hide.
  controller_.OnScrollBegin();
  auto non_inertial = SendTwoScrollUpdates(
      threshold + 1.0f, base::Milliseconds(190), /*is_inertial=*/false);
  EXPECT_FALSE(non_inertial.has_value());
  controller_.OnScrollEnd();

  // Inertial scroll at the same slow speed uses the minimum can-hide region
  // (150), so viewport_offset_y = 200 is eligible to hide.
  controller_.OnScrollBegin();
  auto inertial = SendTwoScrollUpdates(
      threshold + 1.0f, base::Milliseconds(190), /*is_inertial=*/true);
  ASSERT_TRUE(inertial.has_value());
  EXPECT_EQ(inertial->action, SnapHint::Action::kCanHide);
  EXPECT_TRUE(inertial->is_scroll_end_or_inertial);
  controller_.OnScrollEnd();
}

TEST_F(BrowserControlsSnapAnimationControllerTest, OnScrollEndUsesVelocity) {
  controller_.OnScrollBegin();
  float threshold = controller_.TriggerThreshold(1.0f);

  // Accumulate enough downward delta, then call OnScrollEnd().
  SendTwoScrollUpdates(threshold + 10.0f, base::Milliseconds(10));
  auto hint = controller_.OnScrollEnd();
  ASSERT_TRUE(hint.has_value());
  EXPECT_EQ(hint->action, SnapHint::Action::kCanHide);
  EXPECT_LT(hint->slowness, 1.0f);
  EXPECT_TRUE(hint->is_scroll_end_or_inertial);
  EXPECT_FLOAT_EQ(controller_.Slowness(), 1.0f);
}

TEST_F(BrowserControlsSnapAnimationControllerTest,
       BottomControlsOnlyHasNoAlwaysShownRegion) {
  client_.set_top_controls_height(0.0f);
  client_.set_bottom_controls_height(100.0f);
  client_.set_viewport_offset_y(0.0f);

  EXPECT_EQ(controller_.AlwaysShownRegionHeight(), 0.0f);
  EXPECT_FALSE(controller_.ViewportInAlwaysShownRegion());
}

}  // namespace
}  // namespace cc
