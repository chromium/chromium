// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace ash {

class DemoAshPixelDiffTest : public AshTestBase {
 public:
  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  // Creates a top level widget with the specified bounds and color.
  std::unique_ptr<views::Widget> CreateWidgetInSolidColor(
      const gfx::Rect& widget_bounds,
      SkColor color) {
    // Create a top level widget.
    auto widget = std::make_unique<views::Widget>();
    views::Widget::InitParams init_params(
        views::Widget::InitParams::TYPE_POPUP);
    init_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    init_params.bounds = gfx::Rect(widget_bounds);
    init_params.parent = Shell::GetPrimaryRootWindow();
    widget->Init(std::move(init_params));
    widget->Show();

    // Set a solid color contents view.
    auto contents_view = std::make_unique<views::View>();
    contents_view->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    contents_view->layer()->SetColor(color);
    widget->SetContentsView(std::move(contents_view));

    return widget;
  }
};

// Create top level widgets at corners of the primary display. Check the
// screenshot on these widgets.
TEST_F(DemoAshPixelDiffTest, VerifyTopLevelWidgets) {
  constexpr gfx::Size kWidgetSize(50, 50);
  auto widget1 = CreateWidgetInSolidColor(gfx::Rect(kWidgetSize), SK_ColorGRAY);
  const gfx::Rect display_bounds = GetPrimaryDisplay().bounds();
  auto widget2 = CreateWidgetInSolidColor(
      gfx::Rect(gfx::Point(0, display_bounds.height() - kWidgetSize.height()),
                kWidgetSize),
      SK_ColorGREEN);
  auto widget3 = CreateWidgetInSolidColor(
      gfx::Rect(gfx::Point(display_bounds.right() - kWidgetSize.width(), 0),
                kWidgetSize),
      SK_ColorBLUE);
  auto widget4 = CreateWidgetInSolidColor(
      gfx::Rect(gfx::Point(display_bounds.right() - kWidgetSize.width(),
                           display_bounds.height() - kWidgetSize.height()),
                kWidgetSize),
      SK_ColorYELLOW);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_widgets.rev_0", widget1.get(), widget2.get(), widget3.get(),
      widget4.get()));
}

}  // namespace ash
