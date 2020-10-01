// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_chip_view.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

HoldingSpaceItemChipView::HoldingSpaceItemChipView(
    HoldingSpaceItemViewDelegate* delegate,
    const HoldingSpaceItem* item)
    : HoldingSpaceItemView(delegate, item) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kHoldingSpaceChipPadding), kHoldingSpaceChipChildSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  SetPreferredSize(gfx::Size(kHoldingSpaceChipWidth, kHoldingSpaceChipHeight));

  image_ = AddChildView(
      std::make_unique<RoundedImageView>(kHoldingSpaceChipIconSize / 2));

  label_ = AddChildView(std::make_unique<views::Label>(item->text()));
  label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  layout->SetFlexForView(label_, 1);

  TrayPopupItemStyle(TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL)
      .SetupLabel(label_);

  // Subscribe to be notified of changes to `item_`'s image.
  image_subscription_ =
      item->image().AddImageSkiaChangedCallback(base::BindRepeating(
          &HoldingSpaceItemChipView::UpdateImage, base::Unretained(this)));

  UpdateImage();

  AddPin(/*parent=*/this);
}

HoldingSpaceItemChipView::~HoldingSpaceItemChipView() = default;

void HoldingSpaceItemChipView::UpdateImage() {
  image_->SetImage(
      item()->image().image_skia(),
      gfx::Size(kHoldingSpaceChipIconSize, kHoldingSpaceChipIconSize));
  SchedulePaint();
}

BEGIN_METADATA(HoldingSpaceItemChipView, HoldingSpaceItemView)
END_METADATA

}  // namespace ash
