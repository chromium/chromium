// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_zero_state_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/picker/model/picker_category.h"
#include "ash/picker/model/picker_model.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

const std::u16string kPlaceholderCategorySectionTitle =
    u"Placeholder Categories";

// TODO: b/316935667 - Get a relevant icon for each category.
const gfx::VectorIcon& kPlaceholderIcon = kImeMenuEmoticonIcon;

}  // namespace

PickerZeroStateView::PickerZeroStateView(
    SelectCategoryCallback select_category_callback) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  // TODO: b/316935911 - Get actual sections for the categories.
  auto* section_view = AddChildView(
      std::make_unique<PickerSectionView>(kPlaceholderCategorySectionTitle));
  for (auto category : PickerModel().GetAvailableCategories()) {
    auto item_view = std::make_unique<PickerItemView>(
        base::BindRepeating(select_category_callback, category));
    item_view->SetPrimaryText(GetStringForPickerCategory(category));
    item_view->SetLeadingIcon(kPlaceholderIcon);
    section_view->AddItemView(std::move(item_view));
  }
  section_views_.push_back(section_view);
}

PickerZeroStateView::~PickerZeroStateView() = default;

BEGIN_METADATA(PickerZeroStateView, views::View)
END_METADATA

}  // namespace ash
