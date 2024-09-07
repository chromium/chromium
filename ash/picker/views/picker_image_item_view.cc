// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_image_item_view.h"

#include <memory>
#include <utility>

#include "ash/picker/views/picker_item_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace {

constexpr int kPickerImageItemCornerRadius = 8;

}  // namespace

PickerImageItemView::PickerImageItemView(
    std::unique_ptr<views::ImageView> image,
    std::u16string accessible_name,
    SelectItemCallback select_item_callback)
    : PickerItemView(std::move(select_item_callback),
                     FocusIndicatorStyle::kFocusRingWithInsetGap) {
  SetUseDefaultFillLayout(true);
  SetCornerRadius(kPickerImageItemCornerRadius);
  GetViewAccessibility().SetName(std::move(accessible_name));

  image_view_ = AddChildView(std::move(image));
  image_view_->SetCanProcessEventsWithinSubtree(false);
}

PickerImageItemView::~PickerImageItemView() = default;

BEGIN_METADATA(PickerImageItemView)
END_METADATA

}  // namespace ash
