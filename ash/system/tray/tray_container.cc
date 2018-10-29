// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_container.h"

#include <utility>

#include "ash/shelf/shelf.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

TrayContainer::TrayContainer(Shelf* shelf) : shelf_(shelf) {
  DCHECK(shelf_);

  UpdateLayout();
}

TrayContainer::~TrayContainer() = default;

void TrayContainer::UpdateAfterShelfAlignmentChange() {
  UpdateLayout();
}

void TrayContainer::SetMargin(int main_axis_margin, int cross_axis_margin) {
  main_axis_margin_ = main_axis_margin;
  cross_axis_margin_ = cross_axis_margin;
  UpdateLayout();
}

void TrayContainer::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void TrayContainer::ChildVisibilityChanged(View* child) {
  PreferredSizeChanged();
}

void TrayContainer::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.parent == this)
    PreferredSizeChanged();
}

gfx::Rect TrayContainer::GetAnchorBoundsInScreen() const {
  if (shelf_->IsHorizontalAlignment()) {
    // When the virtual keyboard is up, any anchored widgets should anchor to
    // the virtual keyboard instead because it will cover the shelf.
    const gfx::Rect occluded_bounds =
        keyboard::KeyboardController::Get()->GetWorkspaceOccludedBounds();
    if (!occluded_bounds.IsEmpty())
      return occluded_bounds;
  }
  return GetBoundsInScreen();
}

void TrayContainer::UpdateLayout() {
  const bool is_horizontal = shelf_->IsHorizontalAlignment();

  // Adjust the size of status tray dark background by adding additional
  // empty border.
  views::BoxLayout::Orientation orientation =
      is_horizontal ? views::BoxLayout::kHorizontal
                    : views::BoxLayout::kVertical;

  const int hit_region_with_separator =
      kHitRegionPadding + TrayConstants::separator_width();
  gfx::Insets insets(
      is_horizontal
          ? gfx::Insets(0, kHitRegionPadding, 0, hit_region_with_separator)
          : gfx::Insets(kHitRegionPadding, 0, hit_region_with_separator, 0));
  if (base::i18n::IsRTL())
    insets.Set(insets.top(), insets.right(), insets.bottom(), insets.left());
  SetBorder(views::CreateEmptyBorder(insets));

  int horizontal_margin = main_axis_margin_;
  int vertical_margin = cross_axis_margin_;
  if (!is_horizontal)
    std::swap(horizontal_margin, vertical_margin);

  auto layout = std::make_unique<views::BoxLayout>(
      orientation, gfx::Insets(vertical_margin, horizontal_margin),
      kUnifiedTraySpacingBetweenIcons);
  layout->set_minimum_cross_axis_size(kTrayItemSize);
  views::View::SetLayoutManager(std::move(layout));

  PreferredSizeChanged();
}

}  // namespace ash
