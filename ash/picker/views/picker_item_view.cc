// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_item_view.h"

#include <memory>
#include <utility>

#include "ash/picker/views/picker_focus_indicator.h"
#include "ash/style/style_util.h"
#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"

namespace ash {
namespace {

constexpr auto kPickerItemFocusIndicatorMargins = gfx::Insets::VH(6, 0);

std::unique_ptr<views::Background> GetPickerItemBackground(
    PickerItemView::ItemState item_state,
    int corner_radius) {
  switch (item_state) {
    case PickerItemView::ItemState::kNormal:
      return nullptr;
    case PickerItemView::ItemState::kPseudoFocused:
      return views::CreateThemedRoundedRectBackground(
          cros_tokens::kCrosSysHoverOnSubtle, corner_radius);
  }
}

}  // namespace

PickerItemView::PickerItemView(SelectItemCallback select_item_callback)
    : views::Button(select_item_callback),
      select_item_callback_(select_item_callback) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/true);
}

PickerItemView::~PickerItemView() = default;

void PickerItemView::PaintButtonContents(gfx::Canvas* canvas) {
  views::Button::PaintButtonContents(canvas);

  if (item_state_ == ItemState::kPseudoFocused) {
    // TODO: b/326870199 - Check whether grid items should have different focus
    // indicator styling to list items.
    PaintPickerFocusIndicator(
        canvas, gfx::Point(0, kPickerItemFocusIndicatorMargins.top()),
        height() - kPickerItemFocusIndicatorMargins.height(),
        GetColorProvider()->GetColor(cros_tokens::kCrosSysFocusRing));
  }
}

void PickerItemView::SelectItem() {
  select_item_callback_.Run();
}

void PickerItemView::SetCornerRadius(int corner_radius) {
  if (corner_radius_ == corner_radius) {
    return;
  }

  corner_radius_ = corner_radius;
  StyleUtil::InstallRoundedCornerHighlightPathGenerator(
      this, gfx::RoundedCornersF(corner_radius_));
  SetBackground(GetPickerItemBackground(item_state_, corner_radius_));
}

PickerItemView::ItemState PickerItemView::GetItemState() const {
  return item_state_;
}

void PickerItemView::SetItemState(ItemState item_state) {
  if (item_state_ == item_state) {
    return;
  }

  item_state_ = item_state;
  SetBackground(GetPickerItemBackground(item_state_, corner_radius_));
  // Schedule paint to update pseudo focus indicator.
  SchedulePaint();
}

BEGIN_METADATA(PickerItemView)
END_METADATA

}  // namespace ash
