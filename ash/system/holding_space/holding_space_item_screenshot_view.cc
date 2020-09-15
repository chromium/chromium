// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_screenshot_view.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/user/rounded_image_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

HoldingSpaceItemScreenshotView::HoldingSpaceItemScreenshotView(
    const HoldingSpaceItem* item)
    : HoldingSpaceItemView(item) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  image_ =
      AddChildView(std::make_unique<tray::RoundedImageView>(kTrayItemSize / 2));

  // Subscribe to be notified of changes to `item_`'s image.
  image_subscription_ =
      item->image().AddImageSkiaChangedCallback(base::BindRepeating(
          &HoldingSpaceItemScreenshotView::Update, base::Unretained(this)));

  Update();
}

HoldingSpaceItemScreenshotView::~HoldingSpaceItemScreenshotView() = default;

void HoldingSpaceItemScreenshotView::Update() {
  image_->SetImage(item()->image().image_skia(), kHoldingSpaceScreenshotSize);
}

BEGIN_METADATA(HoldingSpaceItemScreenshotView, HoldingSpaceItemView)
END_METADATA

}  // namespace ash
