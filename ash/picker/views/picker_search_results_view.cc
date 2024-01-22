// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_results_view.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/views/picker_gif_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace ash {
namespace {

// TODO: b/316935667 - Get a relevant icon for each search result.
const gfx::VectorIcon& kPlaceholderIcon = vector_icons::kGoogleColorIcon;

}  // namespace

PickerSearchResultsView::PickerSearchResultsView(
    SelectSearchResultCallback select_search_result_callback,
    PickerAssetFetcher* asset_fetcher)
    : select_search_result_callback_(std::move(select_search_result_callback)),
      asset_fetcher_(asset_fetcher) {
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
      section_view->AddItemView(CreateItemView(result));
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

std::unique_ptr<PickerItemView> PickerSearchResultsView::CreateItemView(
    const PickerSearchResult& result) {
  return std::visit(
      base::Overloaded{
          [&, this](const PickerSearchResult::TextData& data) {
            auto item_view = std::make_unique<PickerItemView>(
                base::BindOnce(&PickerSearchResultsView::SelectSearchResult,
                               base::Unretained(this), result));
            item_view->SetPrimaryText(data.text);
            item_view->SetLeadingIcon(kPlaceholderIcon);
            return item_view;
          },
          [&, this](const PickerSearchResult::GifData& data) {
            auto item_view = std::make_unique<PickerItemView>(
                base::BindOnce(&PickerSearchResultsView::SelectSearchResult,
                               base::Unretained(this), result));
            // TODO: b/316936418 - Get gif dimensions to determine size.
            constexpr gfx::Size kPlaceholderGifSize(200, 200);
            // `base::Unretained` is safe here because `this` owns the item
            // views and `asset_fetcher_` outlives `this`.
            item_view->SetPrimaryImage(std::make_unique<PickerGifView>(
                base::BindRepeating(&PickerAssetFetcher::FetchGifFromUrl,
                                    base::Unretained(asset_fetcher_), data.url),
                kPlaceholderGifSize));
            return item_view;
          },
      },
      result.data());
}

BEGIN_METADATA(PickerSearchResultsView, views::View)
END_METADATA

}  // namespace ash
