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
#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_search_results_view_delegate.h"
#include "ash/picker/views/picker_section_list_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
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

constexpr auto kNoResultsViewLabelMargin = gfx::Insets::VH(32, 16);

constexpr int kMaxIndexForMetrics = 10;

}  // namespace

PickerSearchResultsView::PickerSearchResultsView(
    PickerSearchResultsViewDelegate* delegate,
    int picker_view_width,
    PickerAssetFetcher* asset_fetcher)
    : delegate_(delegate) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  SetProperty(views::kElementIdentifierKey, kPickerSearchResultsPageElementId);

  section_list_view_ = AddChildView(std::make_unique<PickerSectionListView>(
      picker_view_width, asset_fetcher));
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
    if (views::View* item =
            section_list_view_->GetItemAbove(pseudo_focused_view_)) {
      SetPseudoFocusedView(item);
      return true;
    }
  }

  // Default to backward pseudo focus traversal.
  return AdvancePseudoFocus(PseudoFocusDirection::kBackward);
}

bool PickerSearchResultsView::MovePseudoFocusDown() {
  if (pseudo_focused_view_ == nullptr) {
    return false;
  }

  if (views::IsViewClass<PickerItemView>(pseudo_focused_view_)) {
    // Try to move directly to an item below the currently pseudo focused item,
    // i.e. skip non-item views.
    if (views::View* item =
            section_list_view_->GetItemBelow(pseudo_focused_view_)) {
      SetPseudoFocusedView(item);
      return true;
    }
  }

  // Default to forward pseudo focus traversal.
  return AdvancePseudoFocus(PseudoFocusDirection::kForward);
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
  if (views::View* item =
          section_list_view_->GetItemLeftOf(pseudo_focused_view_)) {
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
  if (views::View* item =
          section_list_view_->GetItemRightOf(pseudo_focused_view_)) {
    SetPseudoFocusedView(item);
    return true;
  }
  return false;
}

bool PickerSearchResultsView::AdvancePseudoFocus(
    PseudoFocusDirection direction) {
  if (pseudo_focused_view_ == nullptr) {
    return false;
  }

  views::View* view = GetFocusManager()->GetNextFocusableView(
      pseudo_focused_view_, GetWidget(),
      direction == PseudoFocusDirection::kBackward,
      /*dont_loop=*/false);
  if (view == nullptr || !Contains(view)) {
    return false;
  }
  SetPseudoFocusedView(view);
  return true;
}

bool PickerSearchResultsView::GainPseudoFocus(PseudoFocusDirection direction) {
  views::View* view = GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), direction == PseudoFocusDirection::kBackward,
      /*dont_loop=*/false);
  if (view == nullptr || !Contains(view)) {
    return false;
  }
  SetPseudoFocusedView(view);
  return true;
}

void PickerSearchResultsView::LosePseudoFocus() {
  SetPseudoFocusedView(nullptr);
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
  PickerItemView* view = section_view->AddResult(
      result, &preview_controller_,
      base::BindRepeating(&PickerSearchResultsView::SelectSearchResult,
                          base::Unretained(this), result));

  if (auto* list_item_view = views::AsViewClass<PickerListItemView>(view)) {
    list_item_view->SetBadgeAction(delegate_->GetActionForResult(result));
  }
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

  if (section_list_view_->GetItemAbove(pseudo_focused_view_) == nullptr) {
    // For items at the top, scroll all the way up to let users see that they
    // have reached the top of the zero state view.
    ScrollRectToVisible(gfx::Rect(GetLocalBounds().origin(), gfx::Size()));
  } else if (section_list_view_->GetItemBelow(pseudo_focused_view_) ==
             nullptr) {
    // For items at the bottom, scroll all the way down to let users see that
    // they have reached the bottom of the zero state view.
    ScrollRectToVisible(gfx::Rect(GetLocalBounds().bottom_left(), gfx::Size()));
  } else {
    // Otherwise, just ensure the item is visible.
    pseudo_focused_view_->ScrollViewToVisible();
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
