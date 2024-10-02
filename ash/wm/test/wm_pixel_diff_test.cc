// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_cycle/window_cycle_list.h"
#include "ash/wm/window_cycle/window_cycle_view.h"
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ash/wm/window_restore/informed_restore_controller.h"
#include "ash/wm/window_state.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
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
  WmPixelDiffTest() {
    scoped_features_.InitWithFeatures(
        {features::kForestFeature, features::kSavedDeskUiRevamp,
         features::kDeskBarWindowOcclusionOptimization,
         chromeos::features::kOverviewSessionInitOptimizations},
        {});
  }

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
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
      /*revision_number=*/16, desk_widget, overview_widget1, overview_widget2,
      overview_widget3));
}

// TODO(crbug.com/40929874): Test is flaky.
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
      /*revision_number=*/21, widget));
}

TEST_F(WmPixelDiffTest, InformedRestoreNoScreenshotDialog) {
  UpdateDisplay("1600x1000");

  // Chrome apps are unique as they show tab info additionally. Create one
  // chrome info with some example favicons.
  InformedRestoreContentsData::AppInfo chrome_app_info(
      app_constants::kChromeAppId, "Chrome", /*window_id=*/1);
  chrome_app_info.tab_urls = std::vector<GURL>(5, GURL("http://example.com"));
  chrome_app_info.tab_count = 23;

  auto data = std::make_unique<InformedRestoreContentsData>();
  data->apps_infos.push_back(chrome_app_info);

  // Add a couple more windows with generic names. This will trigger the
  // overflow view.
  for (int i = 2; i < 12; ++i) {
    data->apps_infos.emplace_back(
        "test_id", base::StrCat({"Window ", base::NumberToString(i)}),
        /*window_id=*/i);
  }

  // Enter the informed restore overview session.
  Shell::Get()->informed_restore_controller()->MaybeStartInformedRestoreSession(
      std::move(data));
  WaitForOverviewEntered();

  OverviewGrid* grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(grid);
  auto* widget = OverviewGridTestApi(grid).informed_restore_widget();
  ASSERT_TRUE(widget);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "informed_restore_no_screenshot",
      /*revision_number=*/1, widget));
}

}  // namespace ash
