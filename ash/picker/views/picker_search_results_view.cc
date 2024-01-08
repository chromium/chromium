// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_results_view.h"

#include <limits>
#include <memory>
#include <optional>

#include "ash/controls/rounded_scroll_bar.h"
#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

// Padding around each search result section.
constexpr auto kSearchResultSectionMargins = gfx::Insets::VH(0, 12);

}  // namespace

PickerSearchResultsView::PickerSearchResultsView(
    SelectSearchResultCallback callback)
    : select_search_result_callback_(std::move(callback)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetBackgroundColor(std::nullopt);
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view_->SetVerticalScrollBar(
      std::make_unique<RoundedScrollBar>(/*horizontal=*/false));
  scroll_view_->SetContents(std::make_unique<views::FlexLayoutView>());
}

PickerSearchResultsView::~PickerSearchResultsView() = default;

void PickerSearchResultsView::SetSearchResults(
    const PickerSearchResults& search_results) {
  search_results_ = search_results;

  auto scroll_contents = std::make_unique<views::FlexLayoutView>();
  scroll_contents->SetOrientation(views::LayoutOrientation::kVertical);
  scroll_contents->SetDefault(views::kMarginsKey, kSearchResultSectionMargins);

  section_views_.clear();
  for (const auto& section : search_results_.sections()) {
    auto* section_view = scroll_contents->AddChildView(
        std::make_unique<PickerSectionView>(section.heading()));
    for (const auto& result : section.results()) {
      section_view->AddItemView(std::make_unique<PickerItemView>(
          base::BindOnce(&PickerSearchResultsView::SelectSearchResult,
                         base::Unretained(this), result),
          result.text()));
    }
    section_views_.push_back(section_view);
  }

  scroll_view_->SetContents(std::move(scroll_contents));
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
