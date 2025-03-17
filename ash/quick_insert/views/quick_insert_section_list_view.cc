// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_section_list_view.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "ash/quick_insert/quick_insert_asset_fetcher.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ash/quick_insert/views/quick_insert_section_view.h"
#include "base/containers/adapters.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

QuickInsertSectionListView::QuickInsertSectionListView(
    int section_width,
    QuickInsertAssetFetcher* asset_fetcher,
    QuickInsertSubmenuController* submenu_controller)
    : section_width_(section_width),
      asset_fetcher_(asset_fetcher),
      submenu_controller_(submenu_controller) {
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::LayoutOrientation::kVertical))
      ->set_cross_axis_alignment(views::LayoutAlignment::kStretch);
}

QuickInsertSectionListView::~QuickInsertSectionListView() = default;

views::View* QuickInsertSectionListView::GetTopItem() {
  for (views::View* section : children()) {
    if (views::View* top_item =
            views::AsViewClass<QuickInsertSectionView>(section)->GetTopItem()) {
      return top_item;
    }
  }
  return nullptr;
}

views::View* QuickInsertSectionListView::GetBottomItem() {
  for (views::View* section : base::Reversed(children())) {
    if (views::View* bottom_item =
            views::AsViewClass<QuickInsertSectionView>(section)
                ->GetBottomItem()) {
      return bottom_item;
    }
  }
  return nullptr;
}

views::View* QuickInsertSectionListView::GetItemAbove(views::View* item) {
  QuickInsertSectionView* section = GetSectionContaining(item);
  if (section == nullptr) {
    return nullptr;
  }

  // First check if there is an item above in the same section.
  if (views::View* item_below = section->GetItemAbove(item)) {
    return item_below;
  }

  // Otherwise, return the bottom item in a previous non-empty section if there
  // is one.
  for (auto section_it =
           std::make_reverse_iterator(std::ranges::find(children(), section));
       section_it != children().rend(); section_it = std::next(section_it)) {
    if (views::View* prev_section_bottom_item =
            views::AsViewClass<QuickInsertSectionView>(section_it->get())
                ->GetBottomItem()) {
      return prev_section_bottom_item;
    }
  }

  return nullptr;
}

views::View* QuickInsertSectionListView::GetItemBelow(views::View* item) {
  QuickInsertSectionView* section = GetSectionContaining(item);
  if (section == nullptr) {
    return nullptr;
  }

  // First check if there is an item below in the same section.
  if (views::View* item_below = section->GetItemBelow(item)) {
    return item_below;
  }

  // Otherwise, return the top item in the next non-empty section if there is
  // one.
  for (auto section_it = std::next(std::ranges::find(children(), section));
       section_it != children().end(); section_it = std::next(section_it)) {
    if (views::View* next_section_top_item =
            views::AsViewClass<QuickInsertSectionView>(section_it->get())
                ->GetTopItem()) {
      return next_section_top_item;
    }
  }
  return nullptr;
}

views::View* QuickInsertSectionListView::GetItemLeftOf(views::View* item) {
  QuickInsertSectionView* section = GetSectionContaining(item);
  return section != nullptr ? section->GetItemLeftOf(item) : nullptr;
}

views::View* QuickInsertSectionListView::GetItemRightOf(views::View* item) {
  QuickInsertSectionView* section = GetSectionContaining(item);
  return section != nullptr ? section->GetItemRightOf(item) : nullptr;
}

QuickInsertSectionView* QuickInsertSectionListView::AddSection() {
  return AddChildView(std::make_unique<QuickInsertSectionView>(
      section_width_, asset_fetcher_, submenu_controller_));
}

QuickInsertSectionView* QuickInsertSectionListView::AddSectionAt(size_t index) {
  return AddChildViewAt(
      std::make_unique<QuickInsertSectionView>(section_width_, asset_fetcher_,
                                               submenu_controller_),
      index);
}

void QuickInsertSectionListView::ClearSectionList() {
  RemoveAllChildViews();
}

QuickInsertSectionView* QuickInsertSectionListView::GetSectionContaining(
    views::View* item) {
  for (views::View* view = item->parent(); view != nullptr;
       view = view->parent()) {
    if (views::IsViewClass<QuickInsertSectionView>(view) &&
        view->parent() == this) {
      return views::AsViewClass<QuickInsertSectionView>(view);
    }
  }
  return nullptr;
}

BEGIN_METADATA(QuickInsertSectionListView)
END_METADATA

}  // namespace ash
