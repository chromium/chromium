// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr gfx::Size kWidgetSize(50, 50);
}  // namespace

namespace ash {

class DemoAshPixelDiffTest : public AshTestBase {
 public:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  // Creates a top level widget on the display having ID `display_id` with the
  // specified bounds and color.
  std::unique_ptr<views::Widget> CreateWidgetInSolidColor(
      int64_t display_id,
      const gfx::Rect& widget_bounds,
      SkColor color) {
    // Find the root window for the specified display.
    aura::Window* const root_window =
        Shell::Get()->GetRootWindowForDisplayId(display_id);
    CHECK(root_window);

    // Create a top level widget.
    auto widget = std::make_unique<views::Widget>();
    views::Widget::InitParams init_params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_POPUP);
    init_params.bounds = gfx::Rect(widget_bounds);
    init_params.parent = root_window;
    widget->Init(std::move(init_params));
    widget->Show();

    // Set a solid color contents view.
    auto contents_view = std::make_unique<views::View>();
    contents_view->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    contents_view->layer()->SetColor(color);
    widget->SetContentsView(std::move(contents_view));

    return widget;
  }

  // Creates top-level widgets at each corner of `display`. Each widget will
  // have size `kWidgetSize` and will be given a unique color. Note that the
  // specified display must be large enough to show all four widgets without
  // any overlap. The returned vector will contain the widgets in the following
  // order: top-left, top-right, bottom-right, bottom-left. .
  std::vector<std::unique_ptr<views::Widget>> CreateWidgetsInCorners(
      const display::Display& display) {
    // `display` should be able to fit all four widgets without overlap.
    const gfx::Rect display_bounds = display.bounds();
    CHECK(display_bounds.width() >= 2 * kWidgetSize.width() &&
          display_bounds.height() >= 2 * kWidgetSize.height());

    // Create the four widgets.
    std::vector<std::unique_ptr<views::Widget>> widgets;

    // Top-left corner.
    const int64_t display_id = display.id();
    widgets.push_back(CreateWidgetInSolidColor(
        display_id, gfx::Rect(kWidgetSize), SK_ColorGRAY));

    // Top-right corner.
    widgets.push_back(CreateWidgetInSolidColor(
        display_id,
        gfx::Rect(gfx::Point(display_bounds.width() - kWidgetSize.width(), 0),
                  kWidgetSize),
        SK_ColorGREEN));

    // Bottom-right corner.
    widgets.push_back(CreateWidgetInSolidColor(
        display_id,
        gfx::Rect(gfx::Point(display_bounds.width() - kWidgetSize.width(),
                             display_bounds.height() - kWidgetSize.height()),
                  kWidgetSize),
        SK_ColorBLUE));

    // Bottom-left corner.
    widgets.push_back(CreateWidgetInSolidColor(
        display_id,
        gfx::Rect(gfx::Point(0, display_bounds.height() - kWidgetSize.height()),
                  kWidgetSize),
        SK_ColorYELLOW));

    return widgets;
  }
};

// Create top level widgets at corners of the primary display. Check the
// screenshot on these widgets.
TEST_F(DemoAshPixelDiffTest, VerifyTopLevelWidgets) {
  // The primary display that is created by default is 800x600 pixels and has a
  // DSF of 1.

  // Create the widgets.
  const auto widgets = CreateWidgetsInCorners(GetPrimaryDisplay());

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_widgets",
      /*revision_number=*/2, widgets[0].get(), widgets[1].get(),
      widgets[2].get(), widgets[3].get()));
}

// Create top level widgets at corners of the primary display, where the primary
// display has non-default device scale factor (DSF). Check the screenshot on
// these widgets.
TEST_F(DemoAshPixelDiffTest, VerifyTopLevelWidgetsForNonDefaultDSF) {
  // Create the display.
  UpdateDisplay("800x600*2");

  // Create the widgets.
  const auto widgets = CreateWidgetsInCorners(GetPrimaryDisplay());

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_widgets",
      /*revision_number=*/0, widgets[0].get(), widgets[1].get(),
      widgets[2].get(), widgets[3].get()));
}

// Create top level widgets at corners of the primary display, for the case
// where there are two displays and the secondary display has a non-default DSF.
// Check the screenshot on these widgets.
TEST_F(DemoAshPixelDiffTest,
       VerifyTopLevelWidgetsOnPrimaryDisplay_SecondaryDisplayHasNonDefaultDSF) {
  // Create the display.
  UpdateDisplay("800x600*1,800x600*2");

  // Create the widgets.
  const auto widgets = CreateWidgetsInCorners(GetPrimaryDisplay());

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_widgets",
      /*revision_number=*/0, widgets[0].get(), widgets[1].get(),
      widgets[2].get(), widgets[3].get()));
}

// Create top level widgets at corners of secondary display. Check the
// screenshot on these widgets.
TEST_F(DemoAshPixelDiffTest, VerifyTopLevelWidgetsOnSecondaryDisplay) {
  // Create the displays.
  UpdateDisplay("800x600*1,800x600*1");

  // Create the widgets.
  const auto widgets = CreateWidgetsInCorners(GetSecondaryDisplay());

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnSecondaryScreen(
      "check_widgets",
      /*revision_number=*/0, widgets[0].get(), widgets[1].get(),
      widgets[2].get(), widgets[3].get()));
}

// Create top level widgets at corners of secondary display, where the secondary
// display has a non-default device scale factor (DSF). Check the screenshot on
// these widgets.
TEST_F(
    DemoAshPixelDiffTest,
    VerifyTopLevelWidgetsOnSecondaryDisplay_SecondaryDisplayHasNonDefaultDSF) {
  // Create the displays.
  UpdateDisplay("800x600*1,800x600*2");

  // Create the widgets.
  const auto widgets = CreateWidgetsInCorners(GetSecondaryDisplay());

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnSecondaryScreen(
      "check_widgets",
      /*revision_number=*/0, widgets[0].get(), widgets[1].get(),
      widgets[2].get(), widgets[3].get()));
}

// Create top level widgets at corners of secondary display, where both the
// primary and secondary displays have non-default device scale factors (DSFs).
// Check the screenshot on these widgets.
TEST_F(DemoAshPixelDiffTest,
       VerifyTopLevelWidgetsOnSecondaryDisplay_BothDisplaysHaveNonDefaultDSF) {
  // Create the displays.
  UpdateDisplay("800x600*2,800x600*2");

  // Create the widgets.
  const auto widgets = CreateWidgetsInCorners(GetSecondaryDisplay());

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnSecondaryScreen(
      "check_widgets",
      /*revision_number=*/0, widgets[0].get(), widgets[1].get(),
      widgets[2].get(), widgets[3].get()));
}

}  // namespace ash
