// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/drop_target_view.h"

#include <algorithm>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/wm/overview/overview_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/layout_provider.h"

namespace ash {
namespace {

constexpr SkColor kDropTargetBackgroundColor =
    SkColorSetARGB(0x24, 0xFF, 0XFF, 0XFF);
constexpr SkColor kDropTargetBorderColor =
    SkColorSetARGB(0x4C, 0xE8, 0XEA, 0XED);
constexpr int kDropTargetBorderThickness = 2;
constexpr int kDropTargetMiddleSize = 96;

// Values for the plus icon.
constexpr SkColor kPlusIconColor = SkColorSetARGB(0xFF, 0xE8, 0XEA, 0XED);
constexpr float kPlusIconSizeFirFraction = 0.5f;
constexpr float kPlusIconSizeSecFraction = 0.267f;
constexpr int kPlusIconMiddleSize = 48;
constexpr int kPlusIconLargestSize = 72;

}  // namespace

DropTargetView::DropTargetView(bool has_plus_icon) {
  const int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kLow);

  background_view_ = AddChildView(std::make_unique<views::View>());
  background_view_->SetBackground(views::CreateRoundedRectBackground(
      kDropTargetBackgroundColor, corner_radius));

  SetBorder(views::CreateRoundedRectBorder(
      kDropTargetBorderThickness, corner_radius, kDropTargetBorderColor));

  if (has_plus_icon) {
    plus_icon_ = AddChildView(std::make_unique<views::ImageView>());
    plus_icon_->SetCanProcessEventsWithinSubtree(false);
    plus_icon_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
    plus_icon_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  }
}

void DropTargetView::UpdateBackgroundVisibility(bool visible) {
  background_view_->SetVisible(visible);
}

void DropTargetView::Layout() {
  const gfx::Rect local_bounds = GetLocalBounds();
  background_view_->SetBoundsRect(local_bounds);

  if (!plus_icon_)
    return;

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

BEGIN_METADATA(DropTargetView, views::View)
END_METADATA

}  // namespace ash
