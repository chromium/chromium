// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_zero_state_view.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/picker/model/picker_category.h"
#include "ash/picker/model/picker_model.h"
#include "ash/picker/views/picker_caps_nudge_view.h"
#include "ash/picker/views/picker_category_type.h"
#include "ash/picker/views/picker_icons.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_strings.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {

PickerZeroStateView::PickerZeroStateView(
    int picker_view_width,
    SelectCategoryCallback select_category_callback)
    : picker_view_width_(picker_view_width) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  AddChildView(std::make_unique<PickerCapsNudgeView>());

  for (auto category : PickerModel().GetAvailableCategories()) {
    auto item_view = std::make_unique<PickerListItemView>(
        base::BindRepeating(select_category_callback, category));
    item_view->SetPrimaryText(GetLabelForPickerCategory(category));
    item_view->SetLeadingIcon(GetIconForPickerCategory(category));
    GetOrCreateSectionView(category)->AddListItem(std::move(item_view));
  }
}

PickerZeroStateView::~PickerZeroStateView() = default;

PickerSectionView* PickerZeroStateView::GetOrCreateSectionView(
    PickerCategory category) {
  const PickerCategoryType category_type = GetPickerCategoryType(category);
  auto section_view_iterator = section_views_.find(category_type);
  if (section_view_iterator != section_views_.end()) {
    return section_view_iterator->second;
  }

  auto* section_view =
      AddChildView(std::make_unique<PickerSectionView>(picker_view_width_));
  section_view->AddTitleLabel(
      GetSectionTitleForPickerCategoryType(category_type));
  section_views_.insert({category_type, section_view});
  return section_view;
}

BEGIN_METADATA(PickerZeroStateView)
END_METADATA

}  // namespace ash
