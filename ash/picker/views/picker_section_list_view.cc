// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_section_list_view.h"

#include <iterator>
#include <memory>
#include <utility>

#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

PickerSectionListView::PickerSectionListView(int section_width)
    : section_width_(section_width) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
}

PickerSectionListView::~PickerSectionListView() = default;

PickerItemView* PickerSectionListView::GetTopItem() {
  return children().empty()
             ? nullptr
             : views::AsViewClass<PickerSectionView>(children().front().get())
                   ->GetTopItem();
}

PickerItemView* PickerSectionListView::GetBottomItem() {
  return children().empty()
             ? nullptr
             : views::AsViewClass<PickerSectionView>(children().back().get())
                   ->GetBottomItem();
}

PickerItemView* PickerSectionListView::GetItemAbove(PickerItemView* item) {
  PickerSectionView* section = GetSectionContaining(item);
  if (section == nullptr) {
    return nullptr;
  }

  // First check if there is an item above in the same section.
  if (PickerItemView* item_below = section->GetItemAbove(item)) {
    return item_below;
  }

  // Otherwise, return the bottom item in a previous non-empty section if there
  // is one.
  for (auto section_it =
           std::make_reverse_iterator(base::ranges::find(children(), section));
       section_it != children().rend(); section_it = std::next(section_it)) {
    if (PickerItemView* prev_section_bottom_item =
            views::AsViewClass<PickerSectionView>(section_it->get())
                ->GetBottomItem()) {
      return prev_section_bottom_item;
    }
  }

  return nullptr;
}

PickerItemView* PickerSectionListView::GetItemBelow(PickerItemView* item) {
  PickerSectionView* section = GetSectionContaining(item);
  if (section == nullptr) {
    return nullptr;
  }

  // First check if there is an item below in the same section.
  if (PickerItemView* item_below = section->GetItemBelow(item)) {
    return item_below;
  }

  // Otherwise, return the top item in the next non-empty section if there is
  // one.
  for (auto section_it = std::next(base::ranges::find(children(), section));
       section_it != children().end(); section_it = std::next(section_it)) {
    if (PickerItemView* next_section_top_item =
            views::AsViewClass<PickerSectionView>(section_it->get())
                ->GetTopItem()) {
      return next_section_top_item;
    }
  }
  return nullptr;
}

PickerItemView* PickerSectionListView::GetItemLeftOf(PickerItemView* item) {
  PickerSectionView* section = GetSectionContaining(item);
  return section != nullptr ? section->GetItemLeftOf(item) : nullptr;
}

PickerItemView* PickerSectionListView::GetItemRightOf(PickerItemView* item) {
  PickerSectionView* section = GetSectionContaining(item);
  return section != nullptr ? section->GetItemRightOf(item) : nullptr;
}

PickerSectionView* PickerSectionListView::AddSection() {
  return AddChildView(std::make_unique<PickerSectionView>(section_width_));
}

PickerSectionView* PickerSectionListView::AddSectionAt(size_t index) {
  return AddChildViewAt(std::make_unique<PickerSectionView>(section_width_),
                        index);
}

void PickerSectionListView::ClearSectionList() {
  RemoveAllChildViews();
}

PickerSectionView* PickerSectionListView::GetSectionContaining(
    PickerItemView* item) {
  for (views::View* view = item->parent(); view != nullptr;
       view = view->parent()) {
    if (views::IsViewClass<PickerSectionView>(view) && view->parent() == this) {
      return views::AsViewClass<PickerSectionView>(view);
    }
  }
  return nullptr;
}

BEGIN_METADATA(PickerSectionListView)
END_METADATA

}  // namespace ash
