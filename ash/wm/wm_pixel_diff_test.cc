// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

// TODO(b/261084863): For now, add some basic tests. Further investigation is
// needed to determine the location of the test files, whether the tests should
// cover more user journeys and whether we should parameterize for RTL,
// dark/light mode, tablet mode, etc.
class WmPixelDiffTest : public AshTestBase {
 public:
  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
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

  // Helper to modify our windows in this pixel test so they are more visible
  // when debugging.
  auto decorate_window = [](aura::Window* window, const std::u16string& title,
                            SkColor color) -> void {
    auto* widget = views::Widget::GetWidgetForNativeWindow(window);
    ASSERT_TRUE(widget);
    widget->client_view()->AddChildView(
        views::Builder<views::View>()
            .SetBackground(views::CreateRoundedRectBackground(color, 4.f))
            .Build());

    // Add a title and an app icon so that the overview header is fully
    // stocked.
    window->SetTitle(title);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    bitmap.eraseColor(SK_ColorCYAN);
    window->SetProperty(aura::client::kAppIconKey,
                        gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
  };

  decorate_window(window1.get(), u"Window1", SK_ColorDKGRAY);
  decorate_window(window2.get(), u"Window2", SK_ColorBLUE);
  decorate_window(window3.get(), u"Window3", SK_ColorGRAY);

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
      /*revision_number=*/0, desk_widget, overview_widget1, overview_widget2,
      overview_widget3));
}

}  // namespace ash
