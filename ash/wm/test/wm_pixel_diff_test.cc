// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_cycle/window_cycle_list.h"
#include "ash/wm/window_cycle/window_cycle_view.h"
#include "ash/wm/window_state.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

// TODO(b/261084863): For now, add some basic tests. Further investigation is
// needed to determine the location of the test files, whether the tests should
// cover more user journeys and whether we should parameterize for RTL,
// dark/light mode, tablet mode, etc.
class WmPixelDiffTest : public AshTestBase {
 public:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }
};

// A basic overview pixel test that shows three overview windows and the virtual
// desks bar.
TEST_F(WmPixelDiffTest, OverviewAndDesksBarBasic) {
  UpdateDisplay("1600x1000");

  // Create a second desk so the desks bar view shows up.
  auto* controller = DesksController::Get();
  controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  controller->desks()[0]->SetName(u"Desk1", /*set_by_user=*/true);
  controller->desks()[1]->SetName(u"Desk2", /*set_by_user=*/true);

  // Create windows of different positions and sizes so they aren't all stacked
  // on top of each other in the desk preview view, and that we can pixel test
  // extreme cases in overview.
  auto window1 = CreateAppWindow(gfx::Rect(300, 300));
  auto window2 = CreateAppWindow(gfx::Rect(600, 600, 500, 200));
  auto window3 = CreateAppWindow(gfx::Rect(100, 400, 100, 600));

  DecorateWindow(window1.get(), u"Window1", SK_ColorDKGRAY);
  DecorateWindow(window2.get(), u"Window2", SK_ColorBLUE);
  DecorateWindow(window3.get(), u"Window3", SK_ColorGRAY);

  EnterOverview();

  auto* desk_widget = const_cast<views::Widget*>(
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_widget());
  auto* overview_widget1 =
      GetOverviewItemForWindow(window1.get())->item_widget();
  auto* overview_widget2 =
      GetOverviewItemForWindow(window2.get())->item_widget();
  auto* overview_widget3 =
      GetOverviewItemForWindow(window3.get())->item_widget();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "overview_and_desks_bar_basic",
      /*revision_number=*/10, desk_widget, overview_widget1, overview_widget2,
      overview_widget3));
}

// TODO(crbug.com/1479278): Test is flaky.
TEST_F(WmPixelDiffTest, DISABLED_OverviewTabletSnap) {
  UpdateDisplay("1600x1000");

  ShellTestApi().SetTabletModeEnabledForTest(true);

  auto window1 = CreateAppWindow(gfx::Rect(300, 300));
  auto window2 = CreateAppWindow(gfx::Rect(300, 300));
  auto window3 = CreateAppWindow(gfx::Rect(300, 300));

  DecorateWindow(window1.get(), u"Window1", SK_ColorDKGRAY);
  DecorateWindow(window2.get(), u"Window2", SK_ColorBLUE);
  DecorateWindow(window3.get(), u"Window3", SK_ColorRED);

  // Minimize `window3` so it tests a different code path than `window2`.
  // Activate and snap `window` and verify we have entered overview with one
  // snapped window.
  WindowState::Get(window3.get())->Minimize();
  WindowSnapWMEvent wm_left_snap_event(WM_EVENT_SNAP_PRIMARY);
  WindowState::Get(window1.get())->Activate();
  WindowState::Get(window1.get())->OnWMEvent(&wm_left_snap_event);
  ASSERT_TRUE(SplitViewController::Get(Shell::GetPrimaryRootWindow())
                  ->InSplitViewMode());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  auto* const snapped_window_widget =
      views::Widget::GetWidgetForNativeWindow(window1.get());
  auto* const overview_widget2 =
      GetOverviewItemForWindow(window2.get())->item_widget();
  auto* const overview_widget3 =
      GetOverviewItemForWindow(window3.get())->item_widget();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "overview_tablet_snap",
      /*revision_number=*/2, snapped_window_widget, overview_widget2,
      overview_widget3));
}

// A basic window cycle pixel test that shows three windows and the window cycle
// tab slider.
TEST_F(WmPixelDiffTest, WindowCycleBasic) {
  UpdateDisplay("1600x1000");

  // Create a second desk so the window cycle tab slider shows up. This slider
  // lets users toggle between seeing windows on the current desk only, or
  // windows on all desks.
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  desks_controller->desks()[0]->SetName(u"Desk1", /*set_by_user=*/true);
  desks_controller->desks()[1]->SetName(u"Desk2", /*set_by_user=*/true);

  // Create a couple windows of different sizes.
  auto window1 = CreateAppWindow(gfx::Rect(300, 300));
  auto window2 = CreateAppWindow(gfx::Rect(500, 200));
  auto window3 = CreateAppWindow(gfx::Rect(100, 600));
  auto window4 = CreateAppWindow(gfx::Rect(800, 600));

  DecorateWindow(window1.get(), u"Window1", SK_ColorDKGRAY);
  DecorateWindow(window2.get(), u"Window2", SK_ColorBLUE);
  DecorateWindow(window3.get(), u"Window3", SK_ColorGRAY);
  DecorateWindow(window4.get(), u"Window4", SK_ColorGREEN);

  // Press alt+tab to bring up the window cycle UI.
  WindowCycleList::SetDisableInitialDelayForTesting(true);
  GetEventGenerator()->PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);

  const WindowCycleView* cycle_view = Shell::Get()
                                          ->window_cycle_controller()
                                          ->window_cycle_list()
                                          ->cycle_view();
  ASSERT_TRUE(cycle_view);
  views::Widget* widget = const_cast<views::Widget*>(cycle_view->GetWidget());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "window_cycle_basic",
      /*revision_number=*/15, widget));
}

}  // namespace ash
