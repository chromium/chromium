// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_category_view.h"

#include <memory>
#include <utility>

#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/views/picker_search_results_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace ash {

PickerCategoryView::PickerCategoryView(
    int picker_view_width,
    PickerSearchResultsView::SelectSearchResultCallback
        select_search_result_callback,
    PickerAssetFetcher* asset_fetcher) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  search_results_view_ = AddChildView(std::make_unique<PickerSearchResultsView>(
      picker_view_width, std::move(select_search_result_callback),
      asset_fetcher));
}

PickerCategoryView::~PickerCategoryView() = default;

void PickerCategoryView::SetResults(const PickerSearchResults& results) {
  search_results_view_->ClearSearchResults();
  search_results_view_->AppendSearchResults(results);
}

BEGIN_METADATA(PickerCategoryView)
END_METADATA

}  // namespace ash
