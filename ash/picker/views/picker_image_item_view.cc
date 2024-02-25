// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_image_item_view.h"

#include <memory>
#include <utility>

#include "ash/picker/views/picker_item_view.h"
#include "ash/style/style_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace {

constexpr auto kPickerImageItemCornerRadius = gfx::RoundedCornersF(8);

}  // namespace

PickerImageItemView::PickerImageItemView(
    SelectItemCallback select_item_callback,
    std::unique_ptr<views::ImageView> image)
    : PickerItemView(std::move(select_item_callback)) {
  SetUseDefaultFillLayout(true);

  image_view_ = AddChildView(std::move(image));
  image_view_->SetCanProcessEventsWithinSubtree(false);

  StyleUtil::InstallRoundedCornerHighlightPathGenerator(
      this, kPickerImageItemCornerRadius);

  // TODO: b/316936418 - Get accessible name for image contents.
  SetAccessibleName(u"image contents");
}

PickerImageItemView::~PickerImageItemView() = default;

void PickerImageItemView::SetImageSizeFromWidth(int width) {
  image_view_->SetImageSize(
      gfx::Size(width, image_view_->GetHeightForWidth(width)));
}

BEGIN_METADATA(PickerImageItemView)
END_METADATA

}  // namespace ash
