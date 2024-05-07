// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_results_view.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "ash/ash_element_identifiers.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_emoticon_item_view.h"
#include "ash/picker/views/picker_gif_view.h"
#include "ash/picker/views/picker_icons.h"
#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_search_results_view_delegate.h"
#include "ash/picker/views/picker_section_list_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

// Some of the icons we use do not have a default size, so we need to manually
// set it.
constexpr int kIconSize = 20;

constexpr auto kNoResultsViewLabelMargin = gfx::Insets::VH(32, 16);

constexpr int kMaxIndexForMetrics = 10;

PickerCategory GetCategoryForEditorData(
    const PickerSearchResult::EditorData& data) {
  switch (data.mode) {
    case PickerSearchResult::EditorData::Mode::kWrite:
      return PickerCategory::kEditorWrite;
    case PickerSearchResult::EditorData::Mode::kRewrite:
      return PickerCategory::kEditorRewrite;
  }
}

}  // namespace

PickerSearchResultsView::PickerSearchResultsView(
    PickerSearchResultsViewDelegate* delegate,
    int picker_view_width,
    PickerAssetFetcher* asset_fetcher)
    : delegate_(delegate), asset_fetcher_(asset_fetcher) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  SetProperty(views::kElementIdentifierKey, kPickerSearchResultsPageElementId);

  section_list_view_ =
      AddChildView(std::make_unique<PickerSectionListView>(picker_view_width));
  no_results_view_ = AddChildView(
      views::Builder<views::Label>(
          bubble_utils::CreateLabel(
              TypographyToken::kCrosBody2,
              l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT),
              cros_tokens::kCrosSysOnSurfaceVariant))
          .SetVisible(false)
          .SetProperty(views::kMarginsKey, kNoResultsViewLabelMargin)
          .SetHorizontalAlignment(gfx::ALIGN_CENTER)
          .Build());
}

PickerSearchResultsView::~PickerSearchResultsView() = default;

bool PickerSearchResultsView::DoPseudoFocusedAction() {
  if (pseudo_focused_view_ == nullptr) {
    return false;
  }

  return DoPickerPseudoFocusedActionOnView(pseudo_focused_view_);
}

bool PickerSearchResultsView::MovePseudoFocusUp() {
  if (pseudo_focused_view_ == nullptr) {
    return false;
  }

  if (views::IsViewClass<PickerItemView>(pseudo_focused_view_)) {
    // Try to move directly to an item above the currently pseudo focused item,
    // i.e. skip non-item views.
    if (PickerItemView* item = section_list_view_->GetItemAbove(
            views::AsViewClass<PickerItemView>(pseudo_focused_view_))) {
      SetPseudoFocusedView(item);
      return true;
    }
  }

  // Default to backward pseudo focus traversal.
  AdvancePseudoFocus(PseudoFocusDirection::kBackward);
  return true;
}

bool PickerSearchResultsView::MovePseudoFocusDown() {
  if (pseudo_focused_view_ == nullptr) {
    return false;
  }

  if (views::IsViewClass<PickerItemView>(pseudo_focused_view_)) {
    // Try to move directly to an item below the currently pseudo focused item,
    // i.e. skip non-item views.
    if (PickerItemView* item = section_list_view_->GetItemBelow(
            views::AsViewClass<PickerItemView>(pseudo_focused_view_))) {
      SetPseudoFocusedView(item);
      return true;
    }
  }

  // Default to forward pseudo focus traversal.
  AdvancePseudoFocus(PseudoFocusDirection::kForward);
  return true;
}

bool PickerSearchResultsView::MovePseudoFocusLeft() {
  if (pseudo_focused_view_ == nullptr ||
      !views::IsViewClass<PickerItemView>(pseudo_focused_view_)) {
    return false;
  }

  // Only allow left pseudo focus movement if there is an item directly to the
  // left of the current pseudo focused item. In other situations, we prefer not
  // to handle the movement here so that it can instead be used for other
  // purposes, e.g. moving the caret in the search field.
  if (PickerItemView* item = section_list_view_->GetItemLeftOf(
          views::AsViewClass<PickerItemView>(pseudo_focused_view_))) {
    SetPseudoFocusedView(item);
    return true;
  }
  return false;
}

bool PickerSearchResultsView::MovePseudoFocusRight() {
  if (pseudo_focused_view_ == nullptr ||
      !views::IsViewClass<PickerItemView>(pseudo_focused_view_)) {
    return false;
  }

  // Only allow right pseudo focus movement if there is an item directly to the
  // right of the current pseudo focused item. In other situations, we prefer
  // not to handle the movement here so that it can instead be used for other
  // purposes, e.g. moving the caret in the search field.
  if (PickerItemView* item = section_list_view_->GetItemRightOf(
          views::AsViewClass<PickerItemView>(pseudo_focused_view_))) {
    SetPseudoFocusedView(item);
    return true;
  }
  return false;
}

void PickerSearchResultsView::AdvancePseudoFocus(
    PseudoFocusDirection direction) {
  if (pseudo_focused_view_ == nullptr) {
    return;
  }

  views::View* view = GetFocusManager()->GetNextFocusableView(
      pseudo_focused_view_, GetWidget(),
      direction == PseudoFocusDirection::kBackward,
      /*dont_loop=*/false);
  // If the next view is outside this PickerSearchResultsView, then loop back to
  // the first (or last) view.
  if (!Contains(view)) {
    view = GetFocusManager()->GetNextFocusableView(
        this, GetWidget(), direction == PseudoFocusDirection::kBackward,
        /*dont_loop=*/false);
  }

  // There can be a short period of time where child views have been added but
  // not drawn yet, so are not considered focusable. The computed `view` may not
  // be valid in these cases. If so, just leave the current pseudo focused view.
  if (view == nullptr || !Contains(view)) {
    return;
  }

  SetPseudoFocusedView(view);
}

void PickerSearchResultsView::ClearSearchResults() {
  delegate_->NotifyPseudoFocusChanged(nullptr);
  pseudo_focused_view_ = nullptr;
  section_views_.clear();
  section_list_view_->ClearSectionList();
  section_list_view_->SetVisible(true);
  no_results_view_->SetVisible(false);
  top_results_.clear();
}

void PickerSearchResultsView::AppendSearchResults(
    PickerSearchResultsSection section) {
  auto* section_view = section_list_view_->AddSection();
  section_view->AddTitleLabel(
      GetSectionTitleForPickerSectionType(section.type()));
  if (section.has_more_results()) {
    section_view->AddTitleTrailingLink(
        l10n_util::GetStringUTF16(IDS_PICKER_SEE_MORE_BUTTON_TEXT),
        base::BindRepeating(&PickerSearchResultsView::OnTrailingLinkClicked,
                            base::Unretained(this), section.type()));
  }
  for (const auto& result : section.results()) {
    AddResultToSection(result, section_view);
    if (top_results_.size() < kMaxIndexForMetrics) {
      top_results_.push_back(result);
    }
  }
  section_views_.push_back(section_view);

  if (pseudo_focused_view_ == nullptr) {
    SetPseudoFocusedView(section_list_view_->GetTopItem());
  }
}

void PickerSearchResultsView::ShowNoResultsFound() {
  no_results_view_->SetVisible(true);
  section_list_view_->SetVisible(false);
}

void PickerSearchResultsView::SelectSearchResult(
    const PickerSearchResult& result) {
  delegate_->SelectSearchResult(result);
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
            item_view->SetPrimaryText(data.primary_text);
            item_view->SetSecondaryText(data.secondary_text);
            item_view->SetLeadingIcon(data.icon);
            section_view->AddListItem(std::move(item_view));
          },
          [&](const PickerSearchResult::SearchRequestData& data) {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(data.text);
            item_view->SetLeadingIcon(data.icon);
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
          [&](const PickerSearchResult::ClipboardData& data) {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            const gfx::VectorIcon* icon = nullptr;
            switch (data.display_format) {
              case PickerSearchResult::ClipboardData::DisplayFormat::kFile:
                icon = &vector_icons::kContentCopyIcon;
                item_view->SetPrimaryText(data.display_text);
                break;
              case PickerSearchResult::ClipboardData::DisplayFormat::kText:
                icon = &chromeos::kTextIcon;
                item_view->SetPrimaryText(data.display_text);
                break;
              case PickerSearchResult::ClipboardData::DisplayFormat::kImage:
                if (!data.display_image.has_value()) {
                  return;
                }
                icon = &chromeos::kFiletypeImageIcon;
                item_view->SetPrimaryImage(
                    std::make_unique<views::ImageView>(*data.display_image));
                break;
              case PickerSearchResult::ClipboardData::DisplayFormat::kHtml:
                icon = &vector_icons::kCodeIcon;
                item_view->SetPrimaryText(
                    l10n_util::GetStringUTF16(IDS_PICKER_HTML_CONTENT));
                break;
            }
            if (icon) {
              item_view->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
                  *icon, cros_tokens::kCrosSysOnSurface, kIconSize));
            }
            section_view->AddListItem(std::move(item_view));
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
          [&](const PickerSearchResult::LocalFileData& data) {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            // TODO: b/330794217 - Add preview once it's available.
            item_view->SetPrimaryText(data.title);
            item_view->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
                chromeos::kFiletypeImageIcon, cros_tokens::kCrosSysOnSurface));
            section_view->AddListItem(std::move(item_view));
          },
          [&](const PickerSearchResult::DriveFileData& data) {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            // TODO: b/330794217 - Add preview once it's available.
            item_view->SetPrimaryText(data.title);
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
          [&](const PickerSearchResult::EditorData& data) {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            const PickerCategory category = GetCategoryForEditorData(data);
            item_view->SetPrimaryText(GetLabelForPickerCategory(category));
            item_view->SetLeadingIcon(GetIconForPickerCategory(category));
            section_view->AddListItem(std::move(item_view));
          },
      },
      result.data());
}

void PickerSearchResultsView::SetPseudoFocusedView(views::View* view) {
  if (pseudo_focused_view_ == view) {
    return;
  }

  RemovePickerPseudoFocusFromView(pseudo_focused_view_);
  pseudo_focused_view_ = view;
  ApplyPickerPseudoFocusToView(pseudo_focused_view_);
  ScrollPseudoFocusedViewToVisible();
  delegate_->NotifyPseudoFocusChanged(view);
}

void PickerSearchResultsView::OnTrailingLinkClicked(
    PickerSectionType section_type,
    const ui::Event& event) {
  delegate_->SelectMoreResults(section_type);
}

void PickerSearchResultsView::ScrollPseudoFocusedViewToVisible() {
  if (pseudo_focused_view_ == nullptr) {
    return;
  }

  if (!views::IsViewClass<PickerItemView>(pseudo_focused_view_)) {
    pseudo_focused_view_->ScrollViewToVisible();
    return;
  }

  auto* pseudo_focused_item =
      views::AsViewClass<PickerItemView>(pseudo_focused_view_);
  if (section_list_view_->GetItemAbove(pseudo_focused_item) == nullptr) {
    // For items at the top, scroll all the way up to let users see that they
    // have reached the top of the zero state view.
    ScrollRectToVisible(gfx::Rect(GetLocalBounds().origin(), gfx::Size()));
  } else if (section_list_view_->GetItemBelow(pseudo_focused_item) == nullptr) {
    // For items at the bottom, scroll all the way down to let users see that
    // they have reached the bottom of the zero state view.
    ScrollRectToVisible(gfx::Rect(GetLocalBounds().bottom_left(), gfx::Size()));
  } else {
    // Otherwise, just ensure the item is visible.
    pseudo_focused_item->ScrollViewToVisible();
  }
}

int PickerSearchResultsView::GetIndex(
    const PickerSearchResult& inserted_result) {
  auto it = base::ranges::find(top_results_, inserted_result);
  if (it == top_results_.end()) {
    return kMaxIndexForMetrics;
  }
  return std::min(kMaxIndexForMetrics,
                  static_cast<int>(it - top_results_.begin()));
}

BEGIN_METADATA(PickerSearchResultsView)
END_METADATA

}  // namespace ash
