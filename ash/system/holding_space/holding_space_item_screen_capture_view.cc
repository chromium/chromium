// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_screen_capture_view.h"

#include "ash/public/cpp/holding_space/holding_space_color_provider.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

HoldingSpaceItemScreenCaptureView::HoldingSpaceItemScreenCaptureView(
    HoldingSpaceItemViewDelegate* delegate,
    const HoldingSpaceItem* item)
    : HoldingSpaceItemView(delegate, item) {
  SetPreferredSize(kHoldingSpaceScreenCaptureSize);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  image_ = AddChildView(std::make_unique<RoundedImageView>(
      kHoldingSpaceCornerRadius, RoundedImageView::Alignment::kLeading));

  // Subscribe to be notified of changes to `item_`'s image.
  image_subscription_ = item->image().AddImageSkiaChangedCallback(
      base::BindRepeating(&HoldingSpaceItemScreenCaptureView::UpdateImage,
                          base::Unretained(this)));

  UpdateImage();

  views::View* pin_button_container =
      AddChildView(std::make_unique<views::View>());

  auto* layout =
      pin_button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kHoldingSpaceScreenCapturePadding));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  views::View* pin = AddPin(pin_button_container);

  // Create contrasting background for the pin icon.
  pin->SetBackground(views::CreateRoundedRectBackground(
      HoldingSpaceColorProvider::Get()->GetBackgroundColor(),
      kHoldingSpaceScreenCapturePinButtonSize.width() / 2));
  pin->SetPreferredSize(kHoldingSpaceScreenCapturePinButtonSize);
}

HoldingSpaceItemScreenCaptureView::~HoldingSpaceItemScreenCaptureView() =
    default;

void HoldingSpaceItemScreenCaptureView::UpdateImage() {
  image_->SetImage(item()->image().image_skia(),
                   kHoldingSpaceScreenCaptureSize);
  SchedulePaint();
}

BEGIN_METADATA(HoldingSpaceItemScreenCaptureView, HoldingSpaceItemView)
END_METADATA

}  // namespace ash
