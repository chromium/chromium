// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_screenshot_view.h"

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/user/rounded_image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

HoldingSpaceScreenshotView::HoldingSpaceScreenshotView(
    const HoldingSpaceItem* item)
    : item_(item) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetFocusBehavior(FocusBehavior::ALWAYS);

  image_ =
      AddChildView(std::make_unique<tray::RoundedImageView>(kTrayItemSize / 2));

  Update();
}

HoldingSpaceScreenshotView::~HoldingSpaceScreenshotView() = default;

const char* HoldingSpaceScreenshotView::GetClassName() const {
  return "HoldingSpaceScreenshotView";
}

void HoldingSpaceScreenshotView::Update() {
  image_->SetImage(item_->image().image_skia(), kHoldingSpaceScreenshotSize);
}

}  // namespace ash
