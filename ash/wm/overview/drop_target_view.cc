// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/drop_target_view.h"

#include <algorithm>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/wm/overview/overview_constants.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/layout_provider.h"

namespace ash {
namespace {

constexpr SkColor kDropTargetBackgroundColor =
    SkColorSetARGB(0xFF, 0xFF, 0XFF, 0XFF);
constexpr SkColor kDropTargetBorderColor =
    SkColorSetARGB(0x4C, 0xE8, 0XEA, 0XED);
constexpr float kDropTargetBackgroundOpacity = 0.14f;
constexpr int kDropTargetBorderThickness = 2;
constexpr int kDropTargetMiddleSize = 96;

// Values for the plus icon.
constexpr SkColor kPlusIconColor = SkColorSetARGB(0xFF, 0xE8, 0XEA, 0XED);
constexpr float kPlusIconSizeFirFraction = 0.5f;
constexpr float kPlusIconSizeSecFraction = 0.267f;
constexpr int kPlusIconMiddleSize = 48;
constexpr int kPlusIconLargestSize = 72;

}  // namespace

class DropTargetView::PlusIconView : public views::ImageView {
 public:
  PlusIconView() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    set_can_process_events_within_subtree(false);
    SetVerticalAlignment(views::ImageView::Alignment::kCenter);
    SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  }
  ~PlusIconView() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(PlusIconView);
};

DropTargetView::DropTargetView(bool has_plus_icon) {
  background_view_ = new views::View();
  background_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  background_view_->layer()->SetColor(kDropTargetBackgroundColor);
  background_view_->layer()->SetOpacity(kDropTargetBackgroundOpacity);
  const int corner_radius =
      views::LayoutProvider::Get()->GetCornerRadiusMetric(views::EMPHASIS_LOW);
  background_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(corner_radius));
  background_view_->layer()->SetIsFastRoundedCorner(true);
  AddChildView(background_view_);

  if (has_plus_icon) {
    plus_icon_ = new PlusIconView();
    AddChildView(plus_icon_);
  }

  SetBorder(views::CreateRoundedRectBorder(
      kDropTargetBorderThickness, corner_radius, kDropTargetBorderColor));
}

void DropTargetView::UpdateBackgroundVisibility(bool visible) {
  if (background_view_->GetVisible() == visible)
    return;
  background_view_->SetVisible(visible);
}

void DropTargetView::Layout() {
  const gfx::Rect local_bounds = GetLocalBounds();
  background_view_->SetBoundsRect(local_bounds);

  if (plus_icon_) {
    const int min_dimension =
        std::min(local_bounds.width(), local_bounds.height());
    int plus_icon_size = 0;
    if (min_dimension <= kDropTargetMiddleSize) {
      plus_icon_size = kPlusIconSizeFirFraction * min_dimension;
    } else {
      plus_icon_size = std::max(
          std::min(static_cast<int>(kPlusIconSizeSecFraction * min_dimension),
                   kPlusIconLargestSize),
          kPlusIconMiddleSize);
    }

    gfx::Rect icon_bounds = local_bounds;
    plus_icon_->SetImage(gfx::CreateVectorIcon(kOverviewDropTargetPlusIcon,
                                               plus_icon_size, kPlusIconColor));
    icon_bounds.ClampToCenteredSize(gfx::Size(plus_icon_size, plus_icon_size));
    plus_icon_->SetBoundsRect(icon_bounds);
  }
}

}  // namespace ash
