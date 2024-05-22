// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/utility/arc_curve_path_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash::util {

namespace {

// Aliases ---------------------------------------------------------------------

using CornerLocation = ArcCurveCorner::CornerLocation;

// Constants -------------------------------------------------------------------

constexpr size_t kConcaveRadius = 16;
constexpr size_t kConvexRadius = 10;
constexpr gfx::Size kArcCurveSize(/*width=*/60, /*height=*/40);
constexpr size_t kRoundedCornerRadius = 5;

// ArcCurveClippedView ---------------------------------------------------------

// A view clipped by the bounds with an arc curve corner.
class ArcCurveClippedView : public views::View {
 public:
  ArcCurveClippedView(CornerLocation location,
                      const std::optional<size_t>& corner_radius)
      : location_(location), corner_radius_(corner_radius) {}

 private:
  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    SetClipPath(GetArcCurveRectPath(
        GetContentsBounds().size(),
        ArcCurveCorner(location_, kArcCurveSize, kConcaveRadius, kConvexRadius),
        corner_radius_));
  }

  const CornerLocation location_;
  const std::optional<size_t> corner_radius_;
};

}  // namespace

class ArcCurvePathUtilPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple<CornerLocation, /*has_rounded_corner=*/bool>> {
 public:
  CornerLocation GetCornerLocation() const { return std::get<0>(GetParam()); }

  std::optional<size_t> GetCornerRadius() const {
    return std::get<1>(GetParam()) ? std::make_optional(kRoundedCornerRadius)
                                   : std::nullopt;
  }

 private:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ArcCurvePathUtilPixelTest,
    testing::Combine(testing::Values(CornerLocation::kBottomLeft,
                                     CornerLocation::kBottomRight,
                                     CornerLocation::kTopLeft,
                                     CornerLocation::kTopRight),
                     testing::Bool()));

TEST_P(ArcCurvePathUtilPixelTest, basic) {
  // Find the root window for the specified display.
  aura::Window* const root_window =
      Shell::Get()->GetRootWindowForDisplayId(GetPrimaryDisplay().id());
  CHECK(root_window);

  // Create a top level widget.
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams init_params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  init_params.bounds = gfx::Rect(gfx::Point(100, 100), gfx::Size(100, 100));
  init_params.parent = root_window;
  widget->Init(std::move(init_params));
  widget->Show();

  // Set a contents view with an arc curve corner.
  auto contents_view = std::make_unique<ArcCurveClippedView>(
      GetCornerLocation(), GetCornerRadius());
  contents_view->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  contents_view->layer()->SetColor(SK_ColorBLUE);
  widget->SetContentsView(std::move(contents_view));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "arc_curve_corner",
      /*revision_number=*/0, widget.get()));
}

}  // namespace ash::util
