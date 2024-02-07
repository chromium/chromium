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
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_emoticon_item_view.h"
#include "ash/picker/views/picker_gif_view.h"
#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace ash {

PickerSearchResultsView::PickerSearchResultsView(
    int picker_view_width,
    SelectSearchResultCallback select_search_result_callback,
    PickerAssetFetcher* asset_fetcher)
    : picker_view_width_(picker_view_width),
      select_search_result_callback_(std::move(select_search_result_callback)),
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
    auto* section_view = AddChildView(std::make_unique<PickerSectionView>(
        picker_view_width_, section.heading()));
    for (const auto& result : section.results()) {
      AddResultToSection(result, section_view);
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

void PickerSearchResultsView::AddResultToSection(
    const PickerSearchResult& result,
    PickerSectionView* section_view) {
  // `base::Unretained` is safe here because `this` will own the item view which
  // takes this callback.
  auto select_result_callback =
      base::BindOnce(&PickerSearchResultsView::SelectSearchResult,
                     base::Unretained(this), result);
  std::visit(
      base::Overloaded{
          [&](const PickerSearchResult::TextData& data) {
            auto item_view = std::make_unique<PickerItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(data.text);
            section_view->AddListItem(std::move(item_view));
          },
          [&](const PickerSearchResult::EmojiData& data) {
            auto emoji_item = std::make_unique<PickerEmojiItemView>(
                std::move(select_result_callback), data.emoji);
            section_view->AddEmojiItem(std::move(emoji_item));
          },
          [&](const PickerSearchResult::SymbolData& data) {
            auto symbol_item = std::make_unique<PickerSymbolItemView>(
                std::move(select_result_callback), data.symbol);
            section_view->AddSymbolItem(std::move(symbol_item));
          },
          [&](const PickerSearchResult::EmoticonData& data) {
            auto emoticon_item = std::make_unique<PickerEmoticonItemView>(
                std::move(select_result_callback), data.emoticon);
            section_view->AddEmoticonItem(std::move(emoticon_item));
          },
          [&, this](const PickerSearchResult::GifData& data) {
            // `base::Unretained` is safe here because `this` will own the gif
            // view and `asset_fetcher_` outlives `this`.
            auto gif_view = std::make_unique<PickerGifView>(
                base::BindRepeating(&PickerAssetFetcher::FetchGifFromUrl,
                                    base::Unretained(asset_fetcher_), data.url),
                data.dimensions);
            auto gif_item_view = std::make_unique<PickerImageItemView>(
                std::move(select_result_callback), std::move(gif_view));
            section_view->AddImageItem(std::move(gif_item_view));
          },
          [&](const PickerSearchResult::BrowsingHistoryData& data) {
            auto item_view = std::make_unique<PickerItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(base::UTF8ToUTF16(data.url.spec()));
            item_view->SetLeadingIcon(data.icon);
            section_view->AddListItem(std::move(item_view));
          },
      },
      result.data());
}

BEGIN_METADATA(PickerSearchResultsView, views::View)
END_METADATA

}  // namespace ash
