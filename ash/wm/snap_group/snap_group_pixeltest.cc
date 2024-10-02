// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_test_util.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_test_util.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_cycle/window_cycle_list.h"
#include "ash/wm/window_cycle/window_cycle_view.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

// Visual regression tests for Snap Groups feature, comparing visuals against
// established benchmarks.
class SnapGroupPixelTest : public AshTestBase {
 public:
  SnapGroupPixelTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kForestFeature, features::kSavedDeskUiRevamp,
         chromeos::features::kOverviewSessionInitOptimizations},
        {});
  }
  SnapGroupPixelTest(const SnapGroupPixelTest&) = delete;
  SnapGroupPixelTest& operator=(const SnapGroupPixelTest&) = delete;
  ~SnapGroupPixelTest() override = default;

 private:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// -----------------------------------------------------------------------------
// Landscape:

// Visual regression test for divider component (default and hover states).
TEST_F(SnapGroupPixelTest, SnapGroupDividerBasic) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  DecorateWindow(w1.get(), /*title=*/u"w1", SK_ColorGREEN);
  auto* w1_widget = views::Widget::GetWidgetForNativeView(w1.get());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  DecorateWindow(w2.get(), /*title=*/u"w2", SK_ColorBLUE);
  auto* w2_widget = views::Widget::GetWidgetForNativeView(w2.get());

  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);

  auto* divider_widget = GetTopmostSnapGroupDivider()->divider_widget();
  ASSERT_TRUE(divider_widget);

  // Verify the snap group divider UI components on default state.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "snap_group_divider_default_state",
      /*revision_number=*/0, divider_widget, w1_widget, w2_widget));

  // Move the mouse to the position that is a off the center(divider handler
  // view).
  event_generator->MoveMouseTo(
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint() +
      gfx::Vector2d(0, 10));

  // Verify the snap group divider UI components on mouse hover.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "snap_group_divider_hover_state",
      /*revision_number=*/0, divider_widget, w1_widget, w2_widget));
}

// Visual regression test partial split screen layout.
TEST_F(SnapGroupPixelTest, PartialSplit) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  DecorateWindow(w1.get(), /*title=*/u"w1", SK_ColorGREEN);
  auto* w1_widget = views::Widget::GetWidgetForNativeView(w1.get());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  DecorateWindow(w2.get(), /*title=*/u"w2", SK_ColorBLUE);
  auto* w2_widget = views::Widget::GetWidgetForNativeView(w2.get());

  SnapOneTestWindow(w1.get(),
                    /*state_type=*/chromeos::WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio);
  VerifySplitViewOverviewSession(w1.get());
  ClickOverviewItem(GetEventGenerator(), w2.get());
  auto* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  auto* divider_widget = GetTopmostSnapGroupDivider()->divider_widget();
  ASSERT_TRUE(divider_widget);

  // Verify the snap group divider UI components on in 2/3 and 1/3 split screen
  // layout.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "snap_group_partial_split",
      /*revision_number=*/0, divider_widget, w1_widget, w2_widget));
}

// Visual regression test for `OverviewGroupItem`.
TEST_F(SnapGroupPixelTest, OverviewGroupItem) {
  base::test::ScopedFeatureList scoped_feature_list{features::kForestFeature};

  ScopedOverviewTransformWindow::SetImmediateCloseForTests(/*immediate=*/true);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  DecorateWindow(w1.get(), /*title=*/u"w1", SK_ColorGREEN);
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  DecorateWindow(w2.get(), /*title=*/u"w2", SK_ColorBLUE);

  SnapTwoTestWindows(w1.get(), /*window2=*/w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  OverviewItemBase* overview_group_item = GetOverviewItemForWindow(w1.get());
  ASSERT_TRUE(overview_group_item);
  auto* group_item_widget = overview_group_item->item_widget();
  ASSERT_TRUE(group_item_widget);

  // Verify the `OverviewGroupItem` visuals.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "overviewgroupitem",
      /*revision_number=*/1, group_item_widget));

  // Verify the visuals after one of the windows in the group got destroyed.
  w2.reset();
  OverviewItemBase* item_after_destruction = GetOverviewItemForWindow(w1.get());
  ASSERT_TRUE(item_after_destruction);
  auto* remaining_item_widget = item_after_destruction->item_widget();
  ASSERT_TRUE(item_after_destruction);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "remaining_item_widget",
      /*revision_number=*/1, remaining_item_widget));
}

// Visual regression test for Snap Group in window cycle view.
TEST_F(SnapGroupPixelTest, WindowCycleView) {
  WindowCycleList::SetDisableInitialDelayForTesting(true);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  DecorateWindow(w1.get(), /*title=*/u"w1", SK_ColorGREEN);
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  DecorateWindow(w2.get(), /*title=*/u"w2", SK_ColorBLUE);

  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  // Explicitly activate the primary-snapped window so that it comes before
  // secondary-snapped window in MRU order, anticipating a future Alt+Tab
  // revamp.
  wm::ActivateWindow(w1.get());

  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  EXPECT_TRUE(window_cycle_controller->IsCycling());

  const WindowCycleView* window_cycle_view =
      window_cycle_controller->window_cycle_list()->cycle_view();
  ASSERT_TRUE(window_cycle_view);

  views::Widget* window_cycle_widget =
      const_cast<views::Widget*>(window_cycle_view->GetWidget());
  ASSERT_TRUE(window_cycle_widget);

  // Verify the visuals with secondary-snapped window gets focused.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "window_cycle_with_snap_group_secondary_focused",
      /*revision_number=*/0, window_cycle_widget));

  // Verify the visuals with primary-snapped window gets focused.
  event_generator->PressAndReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "window_cycle_with_snap_group_primary_focused",
      /*revision_number=*/0, window_cycle_widget));

  // Verify the visuals after one of the windows in the group got destroyed
  // while stepping.
  w2.reset();
  EXPECT_TRUE(window_cycle_controller->IsCycling());
  const WindowCycleView* updated_window_cycle_view =
      window_cycle_controller->window_cycle_list()->cycle_view();
  ASSERT_TRUE(updated_window_cycle_view);

  views::Widget* updated_window_cycle_widget =
      const_cast<views::Widget*>(window_cycle_view->GetWidget());
  ASSERT_TRUE(updated_window_cycle_widget);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "window_cycle_with_snap_group_window_destruction",
      /*revision_number=*/0, updated_window_cycle_widget));
}

// -----------------------------------------------------------------------------
// Portrait:

// Visual regression test for divider component in portrait mode (default and
// hover states).
TEST_F(SnapGroupPixelTest, SnapGroupDividerBasicInPortrait) {
  UpdateDisplay("900x1200");

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  DecorateWindow(w1.get(), /*title=*/u"w1", SK_ColorGREEN);
  auto* w1_widget = views::Widget::GetWidgetForNativeView(w1.get());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  DecorateWindow(w2.get(), /*title=*/u"w2", SK_ColorBLUE);
  auto* w2_widget = views::Widget::GetWidgetForNativeView(w2.get());

  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/false, event_generator);

  auto* divider_widget = GetTopmostSnapGroupDivider()->divider_widget();
  ASSERT_TRUE(divider_widget);

  // Verify the snap group divider UI components on default state in portrait
  // mode.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "snap_group_divider_default_state_in_portrait",
      /*revision_number=*/0, divider_widget, w1_widget, w2_widget));

  // Move the mouse to the position that is a off the center(divider handler
  // view).
  event_generator->MoveMouseTo(
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint() +
      gfx::Vector2d(10, 0));

  // Verify the snap group divider UI components on mouse hover in portrait
  // mode.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "snap_group_divider_hover_state_in_portrait",
      /*revision_number=*/0, divider_widget, w1_widget, w2_widget));
}

// Visual regression test for `OverviewGroupItem` in portrait mode.
TEST_F(SnapGroupPixelTest, OverviewGroupItemInPortrait) {
  base::test::ScopedFeatureList scoped_feature_list{features::kForestFeature};

  UpdateDisplay("900x1200");

  ScopedOverviewTransformWindow::SetImmediateCloseForTests(/*immediate=*/true);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  DecorateWindow(w1.get(), /*title=*/u"w1", SK_ColorGREEN);
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  DecorateWindow(w2.get(), /*title=*/u"w2", SK_ColorBLUE);

  SnapTwoTestWindows(w1.get(), /*window2=*/w2.get(), /*horizontal=*/false,
                     GetEventGenerator());

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  OverviewItemBase* overview_group_item = GetOverviewItemForWindow(w1.get());
  ASSERT_TRUE(overview_group_item);
  auto* group_item_widget = overview_group_item->item_widget();
  ASSERT_TRUE(group_item_widget);

  // Verify the `OverviewGroupItem` visuals in portrait.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "overviewgroupitem_in_portrait",
      /*revision_number=*/1, group_item_widget));
}

// Portrait mode visual regression test for Snap Group visuals in window cycle
// view.
TEST_F(SnapGroupPixelTest, WindowCycleViewInPortrait) {
  UpdateDisplay("900x1200");

  WindowCycleList::SetDisableInitialDelayForTesting(true);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  DecorateWindow(w1.get(), /*title=*/u"w1", SK_ColorGREEN);
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  DecorateWindow(w2.get(), /*title=*/u"w2", SK_ColorBLUE);

  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/false,
                     GetEventGenerator());

  // Explicitly activate the primary-snapped window so that it comes before
  // secondary-snapped window in MRU order, anticipating a future Alt+Tab
  // revamp.
  wm::ActivateWindow(w1.get());

  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  EXPECT_TRUE(window_cycle_controller->IsCycling());

  const WindowCycleView* window_cycle_view =
      window_cycle_controller->window_cycle_list()->cycle_view();
  ASSERT_TRUE(window_cycle_view);

  views::Widget* window_cycle_widget =
      const_cast<views::Widget*>(window_cycle_view->GetWidget());
  ASSERT_TRUE(window_cycle_widget);

  // Verify the visuals with secondary-snapped window gets focused.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "window_cycle_with_snap_group_secondary_focused_in_portrait",
      /*revision_number=*/0, window_cycle_widget));

  // Verify the visuals with primary-snapped window gets focused.
  event_generator->PressAndReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "window_cycle_with_snap_group_primary_focused_in_portrait",
      /*revision_number=*/0, window_cycle_widget));
}

}  // namespace ash
