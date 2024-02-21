// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_list_item_container_view.h"

#include <memory>
#include <utility>

#include "ash/picker/views/picker_list_item_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"

namespace ash {

PickerListItemContainerView::PickerListItemContainerView() {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
}

PickerListItemContainerView::~PickerListItemContainerView() = default;

PickerListItemView* PickerListItemContainerView::AddListItem(
    std::unique_ptr<PickerListItemView> list_item) {
  return AddChildView(std::move(list_item));
}

BEGIN_METADATA(PickerListItemContainerView)
END_METADATA

}  // namespace ash
