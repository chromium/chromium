// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_results_view.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "ash/ash_element_identifiers.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_emoticon_item_view.h"
#include "ash/picker/views/picker_gif_view.h"
#include "ash/picker/views/picker_icons.h"
#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_section_list_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_strings.h"
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
#include "ui/views/view_class_properties.h"

namespace ash {

PickerSearchResultsView::PickerSearchResultsView(
    int picker_view_width,
    SelectSearchResultCallback select_search_result_callback,
    PickerAssetFetcher* asset_fetcher)
    : select_search_result_callback_(std::move(select_search_result_callback)),
      asset_fetcher_(asset_fetcher) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  SetProperty(views::kElementIdentifierKey, kPickerSearchResultsPageElementId);

  section_list_view_ =
      AddChildView(std::make_unique<PickerSectionListView>(picker_view_width));
}

PickerSearchResultsView::~PickerSearchResultsView() = default;

bool PickerSearchResultsView::DoPseudoFocusedAction() {
  if (pseudo_focused_item_ == nullptr) {
    return false;
  }

  pseudo_focused_item_->SelectItem();
  return true;
}

bool PickerSearchResultsView::MovePseudoFocusUp() {
  if (pseudo_focused_item_ == nullptr) {
    return false;
  }

  PickerItemView* item = section_list_view_->GetItemAbove(pseudo_focused_item_);
  if (item == nullptr) {
    // If there's no item above, move pseudo focus to the bottom item.
    item = section_list_view_->GetBottomItem();
  }
  SetPseudoFocusedItem(item);
  return true;
}

bool PickerSearchResultsView::MovePseudoFocusDown() {
  if (pseudo_focused_item_ == nullptr) {
    return false;
  }

  PickerItemView* item = section_list_view_->GetItemBelow(pseudo_focused_item_);
  if (item == nullptr) {
    // If there's no item below, move pseudo focus to the top item.
    item = section_list_view_->GetTopItem();
  }
  SetPseudoFocusedItem(item);
  return true;
}

bool PickerSearchResultsView::MovePseudoFocusLeft() {
  if (pseudo_focused_item_ == nullptr) {
    return false;
  }

  PickerItemView* item =
      section_list_view_->GetItemLeftOf(pseudo_focused_item_);
  if (item == nullptr) {
    return false;
  }
  SetPseudoFocusedItem(item);
  return true;
}

bool PickerSearchResultsView::MovePseudoFocusRight() {
  if (pseudo_focused_item_ == nullptr) {
    return false;
  }

  PickerItemView* item =
      section_list_view_->GetItemRightOf(pseudo_focused_item_);
  if (item == nullptr) {
    return false;
  }
  SetPseudoFocusedItem(item);
  return true;
}

void PickerSearchResultsView::ClearSearchResults() {
  pseudo_focused_item_ = nullptr;
  section_views_.clear();
  section_list_view_->ClearSectionList();
}

void PickerSearchResultsView::AppendSearchResults(
    PickerSearchResultsSection section) {
  auto* section_view = section_list_view_->AddSection();
  section_view->AddTitleLabel(
      GetSectionTitleForPickerSectionType(section.type()));
  for (const auto& result : section.results()) {
    AddResultToSection(result, section_view);
  }
  section_views_.push_back(section_view);

  if (pseudo_focused_item_ == nullptr) {
    SetPseudoFocusedItem(section_list_view_->GetTopItem());
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
      base::BindRepeating(&PickerSearchResultsView::SelectSearchResult,
                          base::Unretained(this), result);
  std::visit(
      base::Overloaded{
          [&](const PickerSearchResult::TextData& data) {
            auto item_view = std::make_unique<PickerListItemView>(
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
                                    base::Unretained(asset_fetcher_),
                                    data.preview_url),
                base::BindRepeating(
                    &PickerAssetFetcher::FetchGifPreviewImageFromUrl,
                    base::Unretained(asset_fetcher_), data.preview_image_url),
                data.preview_dimensions,
                /*accessible_name=*/data.content_description);
            auto gif_item_view = std::make_unique<PickerImageItemView>(
                std::move(select_result_callback), std::move(gif_view));
            section_view->AddImageItem(std::move(gif_item_view));
          },
          [&](const PickerSearchResult::BrowsingHistoryData& data) {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(data.title);
            item_view->SetSecondaryText(base::UTF8ToUTF16(data.url.spec()));
            item_view->SetLeadingIcon(data.icon);
            section_view->AddListItem(std::move(item_view));
          },
          [&](const PickerSearchResult::CategoryData& data) {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(GetLabelForPickerCategory(data.category));
            item_view->SetLeadingIcon(GetIconForPickerCategory(data.category));
            section_view->AddListItem(std::move(item_view));
          },
      },
      result.data());
}

void PickerSearchResultsView::SetPseudoFocusedItem(PickerItemView* item) {
  if (pseudo_focused_item_ == item) {
    return;
  }

  if (pseudo_focused_item_ != nullptr) {
    pseudo_focused_item_->SetItemState(PickerItemView::ItemState::kNormal);
  }

  pseudo_focused_item_ = item;

  if (pseudo_focused_item_ != nullptr) {
    pseudo_focused_item_->SetItemState(
        PickerItemView::ItemState::kPseudoFocused);
    ScrollPseudoFocusedItemToVisible();
  }
}

void PickerSearchResultsView::ScrollPseudoFocusedItemToVisible() {
  if (pseudo_focused_item_ == nullptr) {
    return;
  }

  if (section_list_view_->GetItemAbove(pseudo_focused_item_) == nullptr) {
    // For items at the top, scroll all the way up to let users see that they
    // have reached the top of the zero state view.
    ScrollRectToVisible(gfx::Rect(GetLocalBounds().origin(), gfx::Size()));
  } else if (section_list_view_->GetItemBelow(pseudo_focused_item_) ==
             nullptr) {
    // For items at the bottom, scroll all the way down to let users see that
    // they have reached the bottom of the zero state view.
    ScrollRectToVisible(gfx::Rect(GetLocalBounds().bottom_left(), gfx::Size()));
  } else {
    // Otherwise, just ensure the item is visible.
    pseudo_focused_item_->ScrollViewToVisible();
  }
}

BEGIN_METADATA(PickerSearchResultsView)
END_METADATA

}  // namespace ash
