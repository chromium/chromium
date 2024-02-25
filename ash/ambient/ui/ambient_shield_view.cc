// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_shield_view.h"

#include <array>

#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/background.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Gray gradient from 5% to 50%.
constexpr std::array<SkColor, 2> kDarkModeColors{
    SkColorSetA(gfx::kGoogleGrey900, 12),
    SkColorSetA(gfx::kGoogleGrey900, 128)};

// Gray gradient from 0% to 20%.
constexpr std::array<SkColor, 2> kLightModeColors{
    SkColorSetA(gfx::kGoogleGrey900, 0), SkColorSetA(gfx::kGoogleGrey900, 51)};

class GradientBackground : public views::Background {
 public:
  enum class Orientation {
    kHorizontal,
    kVertical,
    kDiagonalAscending,
    kDiagonalDescending
  };

  GradientBackground(Orientation orientation,
                     SkColor start_color,
                     SkColor end_color)
      : orientation_(orientation),
        start_color_(start_color),
        end_color_(end_color) {}

  GradientBackground(const GradientBackground&) = delete;
  GradientBackground& operator=(const GradientBackground&) = delete;

  ~GradientBackground() override = default;

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    const auto& bounds = view->GetContentsBounds();
    const auto& points = GetPoints(bounds);

    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kSrcOver);
    flags.setShader(gfx::CreateGradientShader(points.front(), points.back(),
                                              start_color_, end_color_));

    canvas->DrawRect(bounds, flags);
  }

 private:
  const std::array<gfx::Point, 2> GetPoints(const gfx::Rect& bounds) const {
    switch (orientation_) {
      case Orientation::kHorizontal:
        return {bounds.left_center(), bounds.right_center()};
      case Orientation::kVertical:
        return {bounds.top_center(), bounds.bottom_center()};
      case Orientation::kDiagonalAscending:
        return {bounds.bottom_left(), bounds.top_right()};
      case Orientation::kDiagonalDescending:
        return {bounds.origin(), bounds.bottom_right()};
    }
  }

  const Orientation orientation_;
  const SkColor start_color_;
  const SkColor end_color_;
};

std::unique_ptr<GradientBackground> CreateGradientBackground(
    GradientBackground::Orientation orientation,
    const std::array<SkColor, 2> colors) {
  return std::make_unique<GradientBackground>(orientation, colors.front(),
                                              colors.back());
}

}  // namespace

AmbientShieldView::AmbientShieldView() {
  SetID(AmbientViewID::kAmbientShieldView);
  InitLayout();
}

AmbientShieldView::~AmbientShieldView() = default;

void AmbientShieldView::InitLayout() {
  const views::FlexSpecification kScaleUnbounded(
      views::MinimumFlexSizeRule::kPreferred,
      views::MaximumFlexSizeRule::kUnbounded);

  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  views::View* top = AddChildView(std::make_unique<views::View>());
  views::View* bottom = AddChildView(std::make_unique<views::View>());

  for (auto* view : {top, bottom})
    view->SetProperty(views::kFlexBehaviorKey, kScaleUnbounded);

  // TODO(b/223270660): Listen for dark/light mode changes.
  bool dark_mode = DarkLightModeControllerImpl::Get()->IsDarkModeEnabled();
  const auto& colors = dark_mode ? kDarkModeColors : kLightModeColors;

  top->SetBackground(views::CreateSolidBackground(colors.front()));
  bottom->SetBackground(CreateGradientBackground(
      GradientBackground::Orientation::kVertical, colors));
}

BEGIN_METADATA(AmbientShieldView)
END_METADATA

}  // namespace ash
