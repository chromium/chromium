// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_list_item_container_view.h"

#include <iterator>
#include <memory>
#include <utility>

#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

PickerListItemContainerView::PickerListItemContainerView() {
  // Lay out items as a full-width vertical list.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
}

PickerListItemContainerView::~PickerListItemContainerView() = default;

views::View* PickerListItemContainerView::GetTopItem() {
  return children().empty() ? nullptr : children().front().get();
}

views::View* PickerListItemContainerView::GetBottomItem() {
  return children().empty() ? nullptr : children().back().get();
}

views::View* PickerListItemContainerView::GetItemAbove(views::View* item) {
  const auto it = base::ranges::find(children(), item);
  return it == children().end() || it == children().begin()
             ? nullptr
             : std::prev(it)->get();
}

views::View* PickerListItemContainerView::GetItemBelow(views::View* item) {
  const auto it = base::ranges::find(children(), item);
  if (it == children().end()) {
    return nullptr;
  }
  const auto next_it = std::next(it);
  return next_it == children().end() ? nullptr : next_it->get();
}

views::View* PickerListItemContainerView::GetItemLeftOf(views::View* item) {
  return nullptr;
}

views::View* PickerListItemContainerView::GetItemRightOf(views::View* item) {
  return nullptr;
}

PickerListItemView* PickerListItemContainerView::AddListItem(
    std::unique_ptr<PickerListItemView> list_item) {
  return AddChildView(std::move(list_item));
}

BEGIN_METADATA(PickerListItemContainerView)
END_METADATA

}  // namespace ash
