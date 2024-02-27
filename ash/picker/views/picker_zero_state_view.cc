// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_zero_state_view.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/picker/model/picker_model.h"
#include "ash/picker/views/picker_caps_nudge_view.h"
#include "ash/picker/views/picker_category_type.h"
#include "ash/picker/views/picker_icons.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_section_list_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {

PickerZeroStateView::PickerZeroStateView(
    int picker_view_width,
    SelectCategoryCallback select_category_callback) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  caps_nudge_view_ =
      AddChildView(std::make_unique<PickerCapsNudgeView>(base::BindRepeating(
          &PickerZeroStateView::ClearCapsNudge, base::Unretained(this))));

  section_list_view_ =
      AddChildView(std::make_unique<PickerSectionListView>(picker_view_width));
  for (auto category : PickerModel().GetAvailableCategories()) {
    auto item_view = std::make_unique<PickerListItemView>(
        base::BindRepeating(select_category_callback, category));
    item_view->SetPrimaryText(GetLabelForPickerCategory(category));
    item_view->SetLeadingIcon(GetIconForPickerCategory(category));
    GetOrCreateSectionView(category)->AddListItem(std::move(item_view));
  }
  SetPseudoFocusedItem(section_list_view_->GetTopItem());
}

PickerZeroStateView::~PickerZeroStateView() = default;

bool PickerZeroStateView::DoPseudoFocusedAction() {
  if (pseudo_focused_item_ == nullptr) {
    return false;
  }

  pseudo_focused_item_->SelectItem();
  return true;
}

bool PickerZeroStateView::MovePseudoFocusUp() {
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

bool PickerZeroStateView::MovePseudoFocusDown() {
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

bool PickerZeroStateView::MovePseudoFocusLeft() {
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

bool PickerZeroStateView::MovePseudoFocusRight() {
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

PickerSectionView* PickerZeroStateView::GetOrCreateSectionView(
    PickerCategory category) {
  const PickerCategoryType category_type = GetPickerCategoryType(category);
  auto section_view_iterator = section_views_.find(category_type);
  if (section_view_iterator != section_views_.end()) {
    return section_view_iterator->second;
  }

  auto* section_view = section_list_view_->AddSection();
  section_view->AddTitleLabel(
      GetSectionTitleForPickerCategoryType(category_type));
  section_views_.insert({category_type, section_view});
  return section_view;
}

void PickerZeroStateView::ClearCapsNudge() {
  RemoveChildView(caps_nudge_view_);
}

void PickerZeroStateView::SetPseudoFocusedItem(PickerItemView* item) {
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

void PickerZeroStateView::ScrollPseudoFocusedItemToVisible() {
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

BEGIN_METADATA(PickerZeroStateView)
END_METADATA

}  // namespace ash
