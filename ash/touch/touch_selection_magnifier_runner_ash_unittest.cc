// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_selection_magnifier_runner_ash.h"

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_utils.h"
#include "ui/touch_selection/touch_selection_magnifier_runner.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

TouchSelectionMagnifierRunnerAsh* GetMagnifierRunner() {
  return static_cast<TouchSelectionMagnifierRunnerAsh*>(
      ui::TouchSelectionMagnifierRunner::GetInstance());
}

void RunPendingMessages() {
  base::RunLoop().RunUntilIdle();
}

class TouchSelectionMagnifierRunnerAshTest : public NoSessionAshTestBase {
 public:
  TouchSelectionMagnifierRunnerAshTest() = default;

  TouchSelectionMagnifierRunnerAshTest(
      const TouchSelectionMagnifierRunnerAshTest&) = delete;
  TouchSelectionMagnifierRunnerAshTest& operator=(
      const TouchSelectionMagnifierRunnerAshTest&) = delete;

  ~TouchSelectionMagnifierRunnerAshTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kTouchTextEditingRedesign);
    NoSessionAshTestBase::SetUp();
  }

  // Verifies that the magnifier has the correct bounds given the point of
  // interest in context window coordinates.
  // TODO(b/273613374): For now, we assume that the context window, root
  // window, and magnifier parent container have the same bounds, but in
  // practice this might not always be the case. Rewrite these tests once the
  // bounds related parts of the magnifier have been cleaned up.
  void VerifyMagnifierBounds(gfx::PointF point_of_interest) {
    TouchSelectionMagnifierRunnerAsh* magnifier_runner = GetMagnifierRunner();
    ASSERT_TRUE(magnifier_runner);

    const ui::Layer* magnifier_layer =
        magnifier_runner->GetMagnifierLayerForTesting();
    const ui::Layer* zoom_layer = magnifier_runner->GetZoomLayerForTesting();
    ASSERT_TRUE(magnifier_layer);
    ASSERT_TRUE(zoom_layer);

    gfx::Rect zoom_layer_bounds_in_context =
        zoom_layer->bounds() + magnifier_layer->bounds().OffsetFromOrigin();
    EXPECT_EQ(zoom_layer_bounds_in_context.size(),
              TouchSelectionMagnifierRunnerAsh::kMagnifierSize);
    EXPECT_EQ(
        zoom_layer_bounds_in_context.CenterPoint(),
        gfx::Point(
            point_of_interest.x(),
            point_of_interest.y() +
                TouchSelectionMagnifierRunnerAsh::kMagnifierVerticalOffset));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the default touch selection magnifier runner is installed and runs
// when a magnifier should be shown.
TEST_F(TouchSelectionMagnifierRunnerAshTest, InstalledAndRuns) {
  TouchSelectionMagnifierRunnerAsh* magnifier_runner = GetMagnifierRunner();

  // Magnifier runner instance should be installed, but magnifier should not be
  // running initially.
  ASSERT_TRUE(magnifier_runner);
  EXPECT_FALSE(magnifier_runner->IsRunning());

  magnifier_runner->ShowMagnifier(GetContext(), gfx::PointF(300, 200));
  EXPECT_TRUE(magnifier_runner->IsRunning());
  EXPECT_EQ(magnifier_runner->GetCurrentContextForTesting(), GetContext());

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
  EXPECT_FALSE(magnifier_runner->IsRunning());
  EXPECT_FALSE(magnifier_runner->GetCurrentContextForTesting());

  // Show magnifier again.
  magnifier_runner->ShowMagnifier(GetContext(), gfx::PointF(300, 200));
  EXPECT_TRUE(magnifier_runner->IsRunning());
  EXPECT_EQ(magnifier_runner->GetCurrentContextForTesting(), GetContext());

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
  EXPECT_FALSE(magnifier_runner->IsRunning());
  EXPECT_FALSE(magnifier_runner->GetCurrentContextForTesting());
}

// Tests that the touch selection magnifier runner can run again with a
// different context after it is closed.
TEST_F(TouchSelectionMagnifierRunnerAshTest, NewContext) {
  TouchSelectionMagnifierRunnerAsh* magnifier_runner = GetMagnifierRunner();
  ASSERT_TRUE(magnifier_runner);

  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  magnifier_runner->ShowMagnifier(window1.get(), gfx::PointF(300, 200));
  EXPECT_TRUE(magnifier_runner->IsRunning());
  EXPECT_EQ(magnifier_runner->GetCurrentContextForTesting(), window1.get());

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
  EXPECT_FALSE(magnifier_runner->IsRunning());
  EXPECT_FALSE(magnifier_runner->GetCurrentContextForTesting());

  // Show magnifier with different context window.
  magnifier_runner->ShowMagnifier(window2.get(), gfx::PointF(300, 200));
  EXPECT_TRUE(magnifier_runner->IsRunning());
  EXPECT_EQ(magnifier_runner->GetCurrentContextForTesting(), window2.get());

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
  EXPECT_FALSE(magnifier_runner->IsRunning());
  EXPECT_FALSE(magnifier_runner->GetCurrentContextForTesting());
}

// Tests that the magnifier and zoom layers are created and destroyed.
TEST_F(TouchSelectionMagnifierRunnerAshTest, CreatesAndDestroysLayers) {
  TouchSelectionMagnifierRunnerAsh* magnifier_runner = GetMagnifierRunner();
  ASSERT_TRUE(magnifier_runner);

  magnifier_runner->ShowMagnifier(GetContext(), gfx::PointF(300, 200));
  ASSERT_TRUE(magnifier_runner->GetMagnifierLayerForTesting());
  ASSERT_TRUE(magnifier_runner->GetZoomLayerForTesting());

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
  EXPECT_FALSE(magnifier_runner->GetMagnifierLayerForTesting());
  EXPECT_FALSE(magnifier_runner->GetZoomLayerForTesting());
}

// Tests that the magnifier bounds are set correctly.
TEST_F(TouchSelectionMagnifierRunnerAshTest, CorrectBounds) {
  TouchSelectionMagnifierRunnerAsh* magnifier_runner = GetMagnifierRunner();
  ASSERT_TRUE(magnifier_runner);

  gfx::PointF position(300, 200);
  magnifier_runner->ShowMagnifier(GetContext(), position);
  VerifyMagnifierBounds(position);

  // Move the magnifier.
  position = gfx::PointF(400, 150);
  magnifier_runner->ShowMagnifier(GetContext(), position);
  VerifyMagnifierBounds(position);

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
}

}  // namespace

}  // namespace ash
