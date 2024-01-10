// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_category_view.h"

#include <memory>

#include "ash/picker/model/picker_category.h"
#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace ash {

PickerCategoryView::PickerCategoryView(SelectResultCallback callback)
    : select_result_callback_(std::move(callback)) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
}

PickerCategoryView::~PickerCategoryView() = default;

void PickerCategoryView::SetResults(const PickerSearchResults& results) {
  results_ = results;

  section_views_.clear();
  for (const auto& section : results_.sections()) {
    auto* section_view =
        AddChildView(std::make_unique<PickerSectionView>(section.heading()));
    for (const auto& result : section.results()) {
      // `base::Unretained` is safe here because this class owns the section
      // view, which owns the item view.
      section_view->AddItemView(std::make_unique<PickerItemView>(
          base::BindOnce(&PickerCategoryView::SelectResult,
                         base::Unretained(this), result),
          result.text()));
    }
    section_views_.push_back(section_view);
  }
}

void PickerCategoryView::SelectResult(const PickerSearchResult& result) {
  if (!select_result_callback_.is_null()) {
    std::move(select_result_callback_).Run(result);
  }
}

BEGIN_METADATA(PickerCategoryView)
END_METADATA

}  // namespace ash
