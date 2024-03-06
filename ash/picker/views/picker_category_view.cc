// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_category_view.h"

#include <memory>
#include <utility>

#include "ash/picker/model/picker_search_results_section.h"
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

bool PickerCategoryView::DoPseudoFocusedAction() {
  return search_results_view_->DoPseudoFocusedAction();
}

bool PickerCategoryView::MovePseudoFocusUp() {
  return search_results_view_->MovePseudoFocusUp();
}

bool PickerCategoryView::MovePseudoFocusDown() {
  return search_results_view_->MovePseudoFocusDown();
}

bool PickerCategoryView::MovePseudoFocusLeft() {
  return search_results_view_->MovePseudoFocusLeft();
}

bool PickerCategoryView::MovePseudoFocusRight() {
  return search_results_view_->MovePseudoFocusRight();
}

void PickerCategoryView::AdvancePseudoFocus(PseudoFocusDirection direction) {
  search_results_view_->AdvancePseudoFocus(direction);
}

void PickerCategoryView::SetResults(
    std::vector<PickerSearchResultsSection> sections) {
  search_results_view_->ClearSearchResults();
  for (PickerSearchResultsSection& section : sections) {
    search_results_view_->AppendSearchResults(std::move(section));
  }
}

BEGIN_METADATA(PickerCategoryView)
END_METADATA

}  // namespace ash
