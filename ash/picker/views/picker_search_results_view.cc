// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_results_view.h"

#include <memory>

#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace ash {

PickerSearchResultsView::PickerSearchResultsView(
    SelectSearchResultCallback callback)
    : select_search_result_callback_(std::move(callback)) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
}

PickerSearchResultsView::~PickerSearchResultsView() = default;

void PickerSearchResultsView::SetSearchResults(
    const PickerSearchResults& search_results) {
  search_results_ = search_results;

  section_views_.clear();
  RemoveAllChildViews();
  for (const auto& section : search_results_.sections()) {
    auto* section_view =
        AddChildView(std::make_unique<PickerSectionView>(section.heading()));
    for (const auto& result : section.results()) {
      section_view->AddItemView(std::make_unique<PickerItemView>(
          base::BindOnce(&PickerSearchResultsView::SelectSearchResult,
                         base::Unretained(this), result),
          result.text()));
    }
    section_views_.push_back(section_view);
  }
}

void PickerSearchResultsView::SelectSearchResult(
    const PickerSearchResult& result) {
  if (!select_search_result_callback_.is_null()) {
    std::move(select_search_result_callback_).Run(result);
  }
}

BEGIN_METADATA(PickerSearchResultsView, views::View)
END_METADATA

}  // namespace ash
