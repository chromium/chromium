// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_screenshot_view.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

HoldingSpaceItemScreenshotView::HoldingSpaceItemScreenshotView(
    HoldingSpaceItemViewDelegate* delegate,
    const HoldingSpaceItem* item)
    : HoldingSpaceItemView(delegate, item) {
  SetPreferredSize(kHoldingSpaceScreenshotSize);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  image_ = AddChildView(
      std::make_unique<RoundedImageView>(kHoldingSpaceCornerRadius));

  // Subscribe to be notified of changes to `item_`'s image.
  image_subscription_ = item->image().AddImageSkiaChangedCallback(
      base::BindRepeating(&HoldingSpaceItemScreenshotView::UpdateImage,
                          base::Unretained(this)));

  UpdateImage();

  views::View* pin_button_container =
      AddChildView(std::make_unique<views::View>());

  auto* layout =
      pin_button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kHoldingSpaceScreenshotPadding));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  AddPin(pin_button_container);
}

HoldingSpaceItemScreenshotView::~HoldingSpaceItemScreenshotView() = default;

void HoldingSpaceItemScreenshotView::UpdateImage() {
  image_->SetImage(item()->image().image_skia(), kHoldingSpaceScreenshotSize);
  SchedulePaint();
}

BEGIN_METADATA(HoldingSpaceItemScreenshotView, HoldingSpaceItemView)
END_METADATA

}  // namespace ash
