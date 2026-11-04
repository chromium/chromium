// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_container.h"

#include <utility>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

TrayContainer::TrayContainer(Shelf* shelf) : shelf_(shelf) {
  DCHECK(shelf_);

  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  UpdateLayout();
}

TrayContainer::~TrayContainer() {
}

void TrayContainer::CalculateTargetBounds() {
  const LayoutInputs new_layout_inputs = GetLayoutInputs();
  const bool is_horizontal = new_layout_inputs.shelf_alignment_is_horizontal;

  // Adjust the size of status tray dark background by adding additional
  // empty border.
  views::BoxLayout::Orientation orientation =
      is_horizontal ? views::BoxLayout::Orientation::kHorizontal
                    : views::BoxLayout::Orientation::kVertical;

  gfx::Insets insets(
      is_horizontal
          ? gfx::Insets::VH(0, new_layout_inputs.status_area_hit_region_padding)
          : gfx::Insets::VH(new_layout_inputs.status_area_hit_region_padding,
                            0));
  border_ = views::CreateEmptyBorder(insets);

  int horizontal_margin = new_layout_inputs.main_axis_margin;
  int vertical_margin = new_layout_inputs.cross_axis_margin;
  if (!is_horizontal)
    std::swap(horizontal_margin, vertical_margin);

  layout_manager_ = std::make_unique<views::BoxLayout>(
      orientation, gfx::Insets::VH(vertical_margin, horizontal_margin),
      new_layout_inputs.spacing_between_children);
  layout_manager_->set_minimum_cross_axis_size(kTrayItemSize);
}

void TrayContainer::UpdateLayout() {
  const LayoutInputs new_layout_inputs = GetLayoutInputs();
  if (layout_inputs_ == new_layout_inputs)
    return;

  if (border_)
    SetBorder(std::move(border_));

  if (layout_manager_)
    views::View::SetLayoutManager(std::move(layout_manager_));

  DeprecatedLayoutImmediately();
  layout_inputs_ = new_layout_inputs;
}

void TrayContainer::SetMargin(int main_axis_margin, int cross_axis_margin) {
  main_axis_margin_ = main_axis_margin;
  cross_axis_margin_ = cross_axis_margin;
  UpdateLayout();
}

void TrayContainer::SetSpacingBetweenChildren(int space_dip) {
  spacing_between_children_ = space_dip;
  UpdateLayout();
}

void TrayContainer::ChildPreferredSizeChanged(views::View* child) {
  if (layout_manager_)
    UpdateLayout();

  // In the parent View. We will layout in ChildPreferredSizeChanged. But due to
  // the calling order in View::PreferredSizeChanged ChildPreferredSizeChanged
  // happens before InvalidateLayout. This causes the cache of LayoutManagerBase
  // to not be invalidated during layout
  InvalidateLayout();
  PreferredSizeChanged();
}

void TrayContainer::ChildVisibilityChanged(View* child) {
  if (layout_manager_)
    UpdateLayout();

  // Same as ChildPreferredSizeChanged
  InvalidateLayout();
  PreferredSizeChanged();
}

void TrayContainer::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.parent == this)
    PreferredSizeChanged();
}

gfx::Rect TrayContainer::GetAnchorBoundsInScreen() const {
  if (shelf_->IsHorizontalAlignment()) {
    // When the virtual keyboard is up, any anchored widgets should anchor to
    // the virtual keyboard instead because it will cover the shelf.
    const gfx::Rect occluded_bounds =
        keyboard::KeyboardUIController::Get()
            ->GetWorkspaceOccludedBoundsInScreen();
    if (!occluded_bounds.IsEmpty())
      return occluded_bounds;
  }
  return GetBoundsInScreen();
}


TrayContainer::LayoutInputs TrayContainer::GetLayoutInputs() const {
  return {shelf_->IsHorizontalAlignment(),
          ShelfConfig::Get()->status_area_hit_region_padding(),
          GetAnchorBoundsInScreen(),
          main_axis_margin_,
          cross_axis_margin_,
          spacing_between_children_};
}

BEGIN_METADATA(TrayContainer)
END_METADATA

}  // namespace ash
