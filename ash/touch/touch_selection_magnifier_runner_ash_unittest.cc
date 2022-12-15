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

// Tests that the magnifier layer is created and destroyed.
TEST_F(TouchSelectionMagnifierRunnerAshTest, Layer) {
  TouchSelectionMagnifierRunnerAsh* magnifier_runner = GetMagnifierRunner();
  ASSERT_TRUE(magnifier_runner);

  magnifier_runner->ShowMagnifier(GetContext(), gfx::PointF(300, 200));
  ASSERT_TRUE(magnifier_runner->GetMagnifierLayerForTesting());

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
  EXPECT_FALSE(magnifier_runner->GetMagnifierLayerForTesting());
}

// Tests that the magnifier layer is positioned with the correct bounds.
TEST_F(TouchSelectionMagnifierRunnerAshTest, LayerBounds) {
  TouchSelectionMagnifierRunnerAsh* magnifier_runner = GetMagnifierRunner();
  ASSERT_TRUE(magnifier_runner);

  gfx::PointF position(300, 200);
  magnifier_runner->ShowMagnifier(GetContext(), position);
  const ui::Layer* magnifier_layer =
      magnifier_runner->GetMagnifierLayerForTesting();
  ASSERT_TRUE(magnifier_layer);

  gfx::Rect bounds = magnifier_layer->bounds();
  EXPECT_EQ(bounds.size(),
            TouchSelectionMagnifierRunnerAsh::kMagnifierLayerSize);
  EXPECT_EQ(
      bounds.CenterPoint(),
      gfx::Point(
          position.x(),
          position.y() +
              TouchSelectionMagnifierRunnerAsh::kMagnifierVerticalOffset));

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
}

// Tests that the magnifier layer bounds update correctly.
TEST_F(TouchSelectionMagnifierRunnerAshTest, LayerUpdatesBounds) {
  TouchSelectionMagnifierRunnerAsh* magnifier_runner = GetMagnifierRunner();
  ASSERT_TRUE(magnifier_runner);

  gfx::PointF position(300, 200);
  magnifier_runner->ShowMagnifier(GetContext(), position);
  const ui::Layer* magnifier_layer =
      magnifier_runner->GetMagnifierLayerForTesting();
  ASSERT_TRUE(magnifier_layer);

  gfx::Rect bounds = magnifier_layer->bounds();
  EXPECT_EQ(bounds.size(),
            TouchSelectionMagnifierRunnerAsh::kMagnifierLayerSize);
  EXPECT_EQ(
      bounds.CenterPoint(),
      gfx::Point(
          position.x(),
          position.y() +
              TouchSelectionMagnifierRunnerAsh::kMagnifierVerticalOffset));

  // Move the magnifier.
  position = gfx::PointF(400, 150);
  magnifier_runner->ShowMagnifier(GetContext(), position);
  EXPECT_EQ(magnifier_layer, magnifier_runner->GetMagnifierLayerForTesting());

  bounds = magnifier_layer->bounds();
  EXPECT_EQ(bounds.size(),
            TouchSelectionMagnifierRunnerAsh::kMagnifierLayerSize);
  EXPECT_EQ(
      bounds.CenterPoint(),
      gfx::Point(
          position.x(),
          position.y() +
              TouchSelectionMagnifierRunnerAsh::kMagnifierVerticalOffset));

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
}

}  // namespace

}  // namespace ash
