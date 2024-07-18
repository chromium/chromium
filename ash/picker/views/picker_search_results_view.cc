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
#include "ash/picker/views/picker_skeleton_loader_view.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "ash/picker/views/picker_traversable_item_container.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

constexpr gfx::Insets kNoResultsViewInsets(24);
constexpr int kNoResultsIllustrationAndDescriptionSpacing = 16;
constexpr gfx::Size kNoResultsIllustrationSize(200, 100);

constexpr int kMaxIndexForMetrics = 10;

}  // namespace

PickerSearchResultsView::PickerSearchResultsView(
    PickerSearchResultsViewDelegate* delegate,
    int picker_view_width,
    PickerAssetFetcher* asset_fetcher,
    PickerSubmenuController* submenu_controller)
    : delegate_(delegate) {
  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  SetProperty(views::kElementIdentifierKey, kPickerSearchResultsPageElementId);

  section_list_view_ = AddChildView(std::make_unique<PickerSectionListView>(
      picker_view_width, asset_fetcher, submenu_controller));
  no_results_view_ = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetVisible(false)
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetInsideBorderInsets(kNoResultsViewInsets)
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .SetBetweenChildSpacing(kNoResultsIllustrationAndDescriptionSpacing)
          .AddChildren(
              views::Builder<views::ImageView>()
                  .CopyAddressTo(&no_results_illustration_)
                  .SetVisible(false)
                  .SetImageSize(kNoResultsIllustrationSize),
              views::Builder<views::Label>(
                  bubble_utils::CreateLabel(
                      TypographyToken::kCrosBody2,
                      l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT),
                      cros_tokens::kCrosSysOnSurfaceVariant))
                  .CopyAddressTo(&no_results_label_)
                  .SetHorizontalAlignment(gfx::ALIGN_CENTER))
          .Build());

  skeleton_loader_view_ = AddChildView(
      views::Builder<PickerSkeletonLoaderView>().SetVisible(false).Build());
}

PickerSearchResultsView::~PickerSearchResultsView() = default;

views::View* PickerSearchResultsView::GetTopItem() {
  return section_list_view_->GetTopItem();
}

views::View* PickerSearchResultsView::GetBottomItem() {
  return section_list_view_->GetBottomItem();
}

views::View* PickerSearchResultsView::GetItemAbove(views::View* item) {
  if (!Contains(item)) {
    return nullptr;
  }
  if (views::IsViewClass<PickerItemView>(item)) {
    // Skip views that aren't PickerItemViews, to allow users to quickly
    // navigate between items.
    return section_list_view_->GetItemAbove(item);
  }
  views::View* prev_item = GetNextPickerPseudoFocusableView(
      item, PickerPseudoFocusDirection::kBackward, /*should_loop=*/false);
  return Contains(prev_item) ? prev_item : nullptr;
}

views::View* PickerSearchResultsView::GetItemBelow(views::View* item) {
  if (!Contains(item)) {
    return nullptr;
  }
  if (views::IsViewClass<PickerItemView>(item)) {
    // Skip views that aren't PickerItemViews, to allow users to quickly
    // navigate between items.
    return section_list_view_->GetItemBelow(item);
  }
  views::View* next_item = GetNextPickerPseudoFocusableView(
      item, PickerPseudoFocusDirection::kForward, /*should_loop=*/false);
  return Contains(next_item) ? next_item : nullptr;
}

views::View* PickerSearchResultsView::GetItemLeftOf(views::View* item) {
  if (!Contains(item) || !views::IsViewClass<PickerItemView>(item)) {
    return nullptr;
  }
  return section_list_view_->GetItemLeftOf(item);
}

views::View* PickerSearchResultsView::GetItemRightOf(views::View* item) {
  if (!Contains(item) || !views::IsViewClass<PickerItemView>(item)) {
    return nullptr;
  }
  return section_list_view_->GetItemRightOf(item);
}

bool PickerSearchResultsView::ContainsItem(views::View* item) {
  return Contains(item);
}

void PickerSearchResultsView::ClearSearchResults() {
  section_views_.clear();
  section_list_view_->ClearSectionList();
  section_list_view_->SetVisible(true);
  no_results_view_->SetVisible(false);
  StopLoadingAnimation();
  top_results_.clear();
}

void PickerSearchResultsView::AppendSearchResults(
    PickerSearchResultsSection section) {
  StopLoadingAnimation();
  auto* section_view = section_list_view_->AddSection();
  std::u16string section_title =
      GetSectionTitleForPickerSectionType(section.type());
  section_view->AddTitleLabel(section_title);
  if (section.has_more_results()) {
    section_view->AddTitleTrailingLink(
        l10n_util::GetStringUTF16(IDS_PICKER_SEE_MORE_BUTTON_TEXT),
        l10n_util::GetStringFUTF16(IDS_PICKER_SEE_MORE_BUTTON_ACCESSIBLE_NAME,
                                   section_title),
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

  delegate_->RequestPseudoFocus(section_list_view_->GetTopItem());
}

bool PickerSearchResultsView::SearchStopped(ui::ImageModel illustration,
                                            std::u16string description) {
  StopLoadingAnimation();
  if (!section_views_.empty()) {
    return false;
  }
  no_results_illustration_->SetVisible(!illustration.IsEmpty());
  no_results_illustration_->SetImage(std::move(illustration));
  no_results_label_->SetText(std::move(description));
  no_results_view_->SetVisible(true);
  section_list_view_->SetVisible(false);
  return true;
}

void PickerSearchResultsView::ShowLoadingAnimation() {
  ClearSearchResults();
  skeleton_loader_view_->StartAnimationAfter(kLoadingAnimationDelay);
  skeleton_loader_view_->SetVisible(true);
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

void PickerSearchResultsView::OnTrailingLinkClicked(
    PickerSectionType section_type,
    const ui::Event& event) {
  delegate_->SelectMoreResults(section_type);
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

void PickerSearchResultsView::StopLoadingAnimation() {
  skeleton_loader_view_->StopAnimation();
  skeleton_loader_view_->SetVisible(false);
}

BEGIN_METADATA(PickerSearchResultsView)
END_METADATA

}  // namespace ash
