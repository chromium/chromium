// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_zero_state_view.h"

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/picker_clipboard_provider.h"
#include "ash/picker/views/picker_category_type.h"
#include "ash/picker/views/picker_icons.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_section_list_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/picker/views/picker_zero_state_view_delegate.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/components/editor_menu/public/cpp/icon.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {

PickerZeroStateView::PickerZeroStateView(
    PickerZeroStateViewDelegate* delegate,
    base::span<const PickerCategory> available_categories,
    base::span<const PickerCategory> recent_results_categories,
    int picker_view_width,
    PickerAssetFetcher* asset_fetcher)
    : delegate_(delegate) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  section_list_view_ = AddChildView(std::make_unique<PickerSectionListView>(
      picker_view_width, asset_fetcher));

  for (PickerCategory category : recent_results_categories) {
    delegate_->GetZeroStateRecentResults(
        category,
        base::BindRepeating(&PickerZeroStateView::OnFetchRecentResults,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  if (base::Contains(available_categories, PickerCategory::kEditorRewrite)) {
    GetOrCreateSectionView(PickerCategory::kEditorRewrite)->SetVisible(false);

    delegate_->GetSuggestedZeroStateEditorResults(base::BindOnce(
        &PickerZeroStateView::OnFetchZeroStateEditorResults,
        weak_ptr_factory_.GetWeakPtr(), PickerCategory::kEditorRewrite));
  }

  for (PickerCategory category : available_categories) {
    // kEditorRewrite is not visible in the zero-state, since it's replaced with
    // the rewrite suggestions.
    if (category == PickerCategory::kEditorRewrite) {
      continue;
    }

    auto item_view = std::make_unique<PickerListItemView>(
        base::BindRepeating(&PickerZeroStateView::OnCategorySelected,
                            weak_ptr_factory_.GetWeakPtr(), category));
    item_view->SetPrimaryText(GetLabelForPickerCategory(category));
    item_view->SetLeadingIcon(GetIconForPickerCategory(category));
    GetOrCreateSectionView(category)->AddListItem(std::move(item_view));
  }
  SetPseudoFocusedView(section_list_view_->GetTopItem());

  // TODO: b/343092747 - Move this to the top once the `primary_section_view_`
  // always has at least one child.
  if (primary_section_view_ == nullptr) {
    primary_section_view_ = section_list_view_->AddSectionAt(0);
  }
}

PickerZeroStateView::~PickerZeroStateView() = default;

bool PickerZeroStateView::DoPseudoFocusedAction() {
  if (pseudo_focused_view_ == nullptr) {
    return false;
  }

  return DoPickerPseudoFocusedActionOnView(pseudo_focused_view_);
}

bool PickerZeroStateView::MovePseudoFocusUp() {
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

bool PickerZeroStateView::MovePseudoFocusDown() {
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

bool PickerZeroStateView::MovePseudoFocusLeft() {
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

bool PickerZeroStateView::MovePseudoFocusRight() {
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

bool PickerZeroStateView::AdvancePseudoFocus(PseudoFocusDirection direction) {
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

bool PickerZeroStateView::GainPseudoFocus(PseudoFocusDirection direction) {
  views::View* view = GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), direction == PseudoFocusDirection::kBackward,
      /*dont_loop=*/false);
  if (view == nullptr || !Contains(view)) {
    return false;
  }
  SetPseudoFocusedView(view);
  return true;
}

void PickerZeroStateView::LosePseudoFocus() {
  SetPseudoFocusedView(nullptr);
}

PickerSectionView* PickerZeroStateView::GetOrCreateSectionView(
    PickerCategory category) {
  const PickerCategoryType category_type = GetPickerCategoryType(category);
  auto section_view_iterator = category_section_views_.find(category_type);
  if (section_view_iterator != category_section_views_.end()) {
    return section_view_iterator->second;
  }

  auto* section_view = section_list_view_->AddSection();
  section_view->AddTitleLabel(
      GetSectionTitleForPickerCategoryType(category_type));
  category_section_views_.insert({category_type, section_view});
  return section_view;
}

void PickerZeroStateView::OnCategorySelected(PickerCategory category) {
  delegate_->SelectZeroStateCategory(category);
}

void PickerZeroStateView::OnResultSelected(const PickerSearchResult& result) {
  delegate_->SelectZeroStateResult(result);
}

void PickerZeroStateView::SetPseudoFocusedView(views::View* view) {
  if (pseudo_focused_view_ == view) {
    return;
  }

  RemovePickerPseudoFocusFromView(pseudo_focused_view_);
  pseudo_focused_view_ = view;
  ApplyPickerPseudoFocusToView(pseudo_focused_view_);
  ScrollPseudoFocusedViewToVisible();
  delegate_->NotifyPseudoFocusChanged(view);
}

void PickerZeroStateView::ScrollPseudoFocusedViewToVisible() {
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

void PickerZeroStateView::MovePseudoFocusToTopIfNeeded() {
  // TODO: b/345609546 - It would be better to centrally track and check pseudo
  // focus across all Picker views. As is, we can run into situations where
  // multiple views all claim to have a `pseudo_focused_view_`.
  if (pseudo_focused_view_ != nullptr) {
    SetPseudoFocusedView(section_list_view_->GetTopItem());
  }
}

void PickerZeroStateView::OnFetchRecentResults(
    std::vector<PickerSearchResult> results) {
  if (results.empty()) {
    return;
  }
  // TODO: b/343092747 - Remove this to the top once the `primary_section_view_`
  // always has at least one child.
  if (primary_section_view_ == nullptr) {
    primary_section_view_ = section_list_view_->AddSectionAt(0);
  }
  for (const auto& result : results) {
    PickerItemView* view = primary_section_view_->AddResult(
        result, &preview_controller_,
        base::BindRepeating(&PickerZeroStateView::OnResultSelected,
                            weak_ptr_factory_.GetWeakPtr(), result));

    if (auto* list_item_view = views::AsViewClass<PickerListItemView>(view)) {
      list_item_view->SetBadgeAction(delegate_->GetActionForResult(result));
    }
  }
  MovePseudoFocusToTopIfNeeded();
}

void PickerZeroStateView::OnFetchZeroStateEditorResults(
    PickerCategory category,
    std::vector<PickerSearchResult> results) {
  if (results.empty()) {
    return;
  }

  PickerSectionView* section_view = GetOrCreateSectionView(category);
  for (const PickerSearchResult& result : results) {
    const auto* editor_data =
        std::get_if<PickerSearchResult::EditorData>(&result.data());
    CHECK(editor_data);

    auto item_view = std::make_unique<PickerListItemView>(
        base::BindRepeating(&PickerZeroStateView::OnResultSelected,
                            weak_ptr_factory_.GetWeakPtr(), result));
    item_view->SetPrimaryText(editor_data->display_name);
    if (editor_data->category.has_value()) {
      item_view->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
          chromeos::editor_menu::GetIconForPresetQueryCategory(
              *editor_data->category),
          cros_tokens::kCrosSysOnSurface));
    }
    section_view->AddListItem(std::move(item_view));
  }
  section_view->SetVisible(true);
  MovePseudoFocusToTopIfNeeded();
}

BEGIN_METADATA(PickerZeroStateView)
END_METADATA

}  // namespace ash
