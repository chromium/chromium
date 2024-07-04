// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_list_item_container_view.h"

#include <iterator>
#include <memory>
#include <utility>

#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_item_with_submenu_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

std::unique_ptr<views::View> CreateListItemView() {
  auto view = std::make_unique<views::View>();
  view->SetUseDefaultFillLayout(true);
  view->GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
  return view;
}

}  // namespace

PickerListItemContainerView::PickerListItemContainerView() {
  // Lay out items as a full-width vertical list.
  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
}

PickerListItemContainerView::~PickerListItemContainerView() = default;

views::View* PickerListItemContainerView::GetTopItem() {
  return items_.view_size() == 0u ? nullptr : items_.view_at(0u);
}

views::View* PickerListItemContainerView::GetBottomItem() {
  return items_.view_size() == 0u ? nullptr
                                  : items_.view_at(items_.view_size() - 1u);
}

views::View* PickerListItemContainerView::GetItemAbove(views::View* item) {
  const std::optional<size_t> index = items_.GetIndexOfView(item);
  if (!index.has_value() || *index == 0u) {
    return nullptr;
  }

  return items_.view_at(*index - 1u);
}

views::View* PickerListItemContainerView::GetItemBelow(views::View* item) {
  const std::optional<size_t> index = items_.GetIndexOfView(item);
  if (!index.has_value() || *index == items_.view_size() - 1u) {
    return nullptr;
  }

  return items_.view_at(*index + 1u);
}

views::View* PickerListItemContainerView::GetItemLeftOf(views::View* item) {
  return nullptr;
}

views::View* PickerListItemContainerView::GetItemRightOf(views::View* item) {
  return nullptr;
}

bool PickerListItemContainerView::ContainsItem(views::View* item) {
  return items_.GetIndexOfView(item).has_value();
}

PickerListItemView* PickerListItemContainerView::AddListItem(
    std::unique_ptr<PickerListItemView> list_item) {
  items_.Add(list_item.get(), items_.view_size());
  return AddChildView(CreateListItemView())->AddChildView(std::move(list_item));
}

PickerItemWithSubmenuView* PickerListItemContainerView::AddItemWithSubmenu(
    std::unique_ptr<PickerItemWithSubmenuView> item_with_submenu) {
  items_.Add(item_with_submenu.get(), items_.view_size());
  return AddChildView(CreateListItemView())
      ->AddChildView(std::move(item_with_submenu));
}

BEGIN_METADATA(PickerListItemContainerView)
END_METADATA

}  // namespace ash
