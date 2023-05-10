// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_selection_magnifier_runner_ash.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/touch_selection/touch_selection_magnifier_runner.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

gfx::SelectionBound GetSelectionBoundForVerticalCaret(gfx::PointF caret_top,
                                                      float caret_height) {
  gfx::PointF caret_bottom = caret_top;
  caret_bottom.Offset(0, caret_height);
  gfx::SelectionBound caret_bound;
  caret_bound.set_type(gfx::SelectionBound::CENTER);
  caret_bound.SetEdge(caret_top, caret_bottom);
  return caret_bound;
}

TouchSelectionMagnifierRunnerAsh* GetMagnifierRunner() {
  return static_cast<TouchSelectionMagnifierRunnerAsh*>(
      ui::TouchSelectionMagnifierRunner::GetInstance());
}

aura::Window* GetMagnifierParentContainerForRoot(aura::Window* root) {
  return root->GetChildById(kShellWindowId_ImeWindowParentContainer);
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

  void SetMagnifierParentContainerBoundsInContext(
      aura::Window* context,
      const gfx::Rect& bounds_in_context) {
    aura::Window* magnifier_parent_container =
        GetMagnifierParentContainerForRoot(context->GetRootWindow());
    gfx::Rect bounds_in_root(bounds_in_context);
    aura::Window::ConvertRectToTarget(context, context->GetRootWindow(),
                                      &bounds_in_root);
    magnifier_parent_container->SetBounds(bounds_in_root);
  }

  gfx::Rect GetMagnifierLayerBoundsInContext(aura::Window* context) {
    gfx::Rect magnifier_layer_bounds =
        GetMagnifierRunner()->GetMagnifierLayerForTesting()->bounds();
    aura::Window::ConvertRectToTarget(
        GetMagnifierParentContainerForRoot(context->GetRootWindow()), context,
        &magnifier_layer_bounds);
    return magnifier_layer_bounds;
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

  magnifier_runner->ShowMagnifier(
      GetContext(),
      GetSelectionBoundForVerticalCaret(gfx::PointF(300, 200), 10));
  EXPECT_TRUE(magnifier_runner->IsRunning());
  EXPECT_EQ(magnifier_runner->GetCurrentContextForTesting(), GetContext());

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
  EXPECT_FALSE(magnifier_runner->IsRunning());
  EXPECT_FALSE(magnifier_runner->GetCurrentContextForTesting());

  // Show magnifier again.
  magnifier_runner->ShowMagnifier(
      GetContext(),
      GetSelectionBoundForVerticalCaret(gfx::PointF(300, 200), 10));
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

  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  magnifier_runner->ShowMagnifier(
      window1.get(),
      GetSelectionBoundForVerticalCaret(gfx::PointF(300, 200), 10));
  EXPECT_TRUE(magnifier_runner->IsRunning());
  EXPECT_EQ(magnifier_runner->GetCurrentContextForTesting(), window1.get());

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
  EXPECT_FALSE(magnifier_runner->IsRunning());
  EXPECT_FALSE(magnifier_runner->GetCurrentContextForTesting());

  // Show magnifier with different context window.
  magnifier_runner->ShowMagnifier(
      window2.get(),
      GetSelectionBoundForVerticalCaret(gfx::PointF(300, 200), 10));
  EXPECT_TRUE(magnifier_runner->IsRunning());
  EXPECT_EQ(magnifier_runner->GetCurrentContextForTesting(), window2.get());

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
  EXPECT_FALSE(magnifier_runner->IsRunning());
  EXPECT_FALSE(magnifier_runner->GetCurrentContextForTesting());
}

// Tests that the magnifier layer is created and destroyed.
TEST_F(TouchSelectionMagnifierRunnerAshTest, CreatesAndDestroysLayers) {
  TouchSelectionMagnifierRunnerAsh* magnifier_runner = GetMagnifierRunner();
  ASSERT_TRUE(magnifier_runner);

  magnifier_runner->ShowMagnifier(
      GetContext(),
      GetSelectionBoundForVerticalCaret(gfx::PointF(300, 200), 10));
  ASSERT_TRUE(magnifier_runner->GetMagnifierLayerForTesting());

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
  EXPECT_FALSE(magnifier_runner->GetMagnifierLayerForTesting());
}

// Tests that the magnifier is horizontally centered above a vertical caret.
TEST_F(TouchSelectionMagnifierRunnerAshTest, BoundsForVerticalCaret) {
  TouchSelectionMagnifierRunnerAsh* magnifier_runner = GetMagnifierRunner();
  ASSERT_TRUE(magnifier_runner);

  aura::Window* context = GetContext();
  SetMagnifierParentContainerBoundsInContext(context,
                                             gfx::Rect(-50, -50, 800, 800));

  gfx::PointF caret_top(300, 200);
  float caret_height = 10;
  magnifier_runner->ShowMagnifier(
      context, GetSelectionBoundForVerticalCaret(caret_top, caret_height));
  gfx::Rect magnifier_layer_bounds = GetMagnifierLayerBoundsInContext(context);
  EXPECT_EQ(magnifier_layer_bounds.CenterPoint().x(), caret_top.x());
  EXPECT_LT(magnifier_layer_bounds.bottom(), caret_top.y());

  // Move the caret.
  caret_top.Offset(10, -5);
  magnifier_runner->ShowMagnifier(
      context, GetSelectionBoundForVerticalCaret(caret_top, caret_height));
  magnifier_layer_bounds = GetMagnifierLayerBoundsInContext(context);
  EXPECT_EQ(magnifier_layer_bounds.CenterPoint().x(), caret_top.x());
  EXPECT_LT(magnifier_layer_bounds.bottom(), caret_top.y());

  // Show a differently sized caret.
  caret_height = 20;
  magnifier_runner->ShowMagnifier(
      context, GetSelectionBoundForVerticalCaret(caret_top, caret_height));
  magnifier_layer_bounds = GetMagnifierLayerBoundsInContext(context);
  EXPECT_EQ(magnifier_layer_bounds.CenterPoint().x(), caret_top.x());
  EXPECT_LT(magnifier_layer_bounds.bottom(), caret_top.y());

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
}

// Tests that the magnifier stays inside the parent container even when showing
// a caret close to the edge of the parent container.
TEST_F(TouchSelectionMagnifierRunnerAshTest, StaysInsideParentContainer) {
  TouchSelectionMagnifierRunnerAsh* magnifier_runner = GetMagnifierRunner();
  ASSERT_TRUE(magnifier_runner);

  aura::Window* context = GetContext();
  const gfx::Rect parent_container_bounds_in_context(50, 60, 500, 400);
  SetMagnifierParentContainerBoundsInContext(
      context, parent_container_bounds_in_context);

  // Left edge.
  magnifier_runner->ShowMagnifier(
      context, GetSelectionBoundForVerticalCaret(gfx::PointF(60, 200), 10));
  EXPECT_TRUE(parent_container_bounds_in_context.Contains(
      GetMagnifierLayerBoundsInContext(context)));

  // Top edge.
  magnifier_runner->ShowMagnifier(
      context, GetSelectionBoundForVerticalCaret(gfx::PointF(200, 65), 10));
  EXPECT_TRUE(parent_container_bounds_in_context.Contains(
      GetMagnifierLayerBoundsInContext(context)));

  // Right edge.
  magnifier_runner->ShowMagnifier(
      context, GetSelectionBoundForVerticalCaret(gfx::PointF(540, 200), 10));
  EXPECT_TRUE(parent_container_bounds_in_context.Contains(
      GetMagnifierLayerBoundsInContext(context)));

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
}

// Tests that the magnifier remains the same size even at the edge of the parent
// container.
TEST_F(TouchSelectionMagnifierRunnerAshTest, Size) {
  TouchSelectionMagnifierRunnerAsh* magnifier_runner = GetMagnifierRunner();
  ASSERT_TRUE(magnifier_runner);

  aura::Window* context = GetContext();
  const gfx::Rect parent_container_bounds_in_context(50, 60, 500, 400);
  SetMagnifierParentContainerBoundsInContext(
      context, parent_container_bounds_in_context);

  magnifier_runner->ShowMagnifier(
      context, GetSelectionBoundForVerticalCaret(gfx::PointF(300, 200), 10));
  const gfx::Size magnifier_layer_size =
      GetMagnifierLayerBoundsInContext(context).size();

  // Move the caret near the edge of the parent container.
  magnifier_runner->ShowMagnifier(
      context, GetSelectionBoundForVerticalCaret(gfx::PointF(55, 65), 10));
  EXPECT_EQ(GetMagnifierLayerBoundsInContext(context).size(),
            magnifier_layer_size);

  magnifier_runner->CloseMagnifier();
  RunPendingMessages();
}

}  // namespace

}  // namespace ash
