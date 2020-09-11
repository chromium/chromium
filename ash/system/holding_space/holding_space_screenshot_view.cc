// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_screenshot_view.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/user/rounded_image_view.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

HoldingSpaceScreenshotView::HoldingSpaceScreenshotView(
    const HoldingSpaceItem* item)
    : item_(item) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  GetViewAccessibility().OverrideName(item_->text());
  SetFocusBehavior(FocusBehavior::ALWAYS);

  image_ =
      AddChildView(std::make_unique<tray::RoundedImageView>(kTrayItemSize / 2));

  // Subscribe to be notified of changes to `item_`'s image.
  image_subscription_ =
      item_->image().AddImageSkiaChangedCallback(base::BindRepeating(
          &HoldingSpaceScreenshotView::Update, base::Unretained(this)));

  Update();
}

HoldingSpaceScreenshotView::~HoldingSpaceScreenshotView() = default;

int HoldingSpaceScreenshotView::GetDragOperations(const gfx::Point& point) {
  return ui::DragDropTypes::DRAG_COPY;
}

void HoldingSpaceScreenshotView::WriteDragData(const gfx::Point& point,
                                               ui::OSExchangeData* data) {
  data->SetFilename(item_->file_path());
}

void HoldingSpaceScreenshotView::Update() {
  image_->SetImage(item_->image().image_skia(), kHoldingSpaceScreenshotSize);
}

BEGIN_METADATA(HoldingSpaceScreenshotView, views::View)
END_METADATA

}  // namespace ash
