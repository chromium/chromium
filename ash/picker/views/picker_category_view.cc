// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_category_view.h"

#include <memory>
#include <utility>

#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/views/picker_search_results_view.h"
#include "ash/picker/views/picker_skeleton_loader_view.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace ash {

PickerCategoryView::PickerCategoryView(
    PickerSearchResultsViewDelegate* delegate,
    int picker_view_width,
    PickerAssetFetcher* asset_fetcher) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  search_results_view_ = AddChildView(std::make_unique<PickerSearchResultsView>(
      delegate, picker_view_width, asset_fetcher));

  skeleton_loader_view_ = AddChildView(
      views::Builder<PickerSkeletonLoaderView>().SetVisible(false).Build());
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

bool PickerCategoryView::AdvancePseudoFocus(PseudoFocusDirection direction) {
  return search_results_view_->AdvancePseudoFocus(direction);
}

bool PickerCategoryView::GainPseudoFocus(PseudoFocusDirection direction) {
  return search_results_view_->GainPseudoFocus(direction);
}

void PickerCategoryView::LosePseudoFocus() {
  search_results_view_->LosePseudoFocus();
}

void PickerCategoryView::ShowLoadingAnimation() {
  search_results_view_->ClearSearchResults();
  search_results_view_->SetVisible(false);

  skeleton_loader_view_->StartAnimationAfter(kLoadingAnimationDelay);
  skeleton_loader_view_->SetVisible(true);
}

void PickerCategoryView::SetResults(
    std::vector<PickerSearchResultsSection> sections) {
  skeleton_loader_view_->StopAnimation();
  skeleton_loader_view_->SetVisible(false);

  search_results_view_->ClearSearchResults();
  if (sections.empty()) {
    search_results_view_->ShowNoResultsFound();
  } else {
    for (PickerSearchResultsSection& section : sections) {
      search_results_view_->AppendSearchResults(std::move(section));
    }
  }
  search_results_view_->SetVisible(true);
}

BEGIN_METADATA(PickerCategoryView)
END_METADATA

}  // namespace ash
