// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_LIST_ITEM_CONTAINER_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_LIST_ITEM_CONTAINER_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace ash {

class PickerItemWithSubmenuView;
class PickerListItemView;

// Container view for the list items in a section. The list items are displayed
// in a vertical list, each spanning the width of the container.
class ASH_EXPORT PickerListItemContainerView
    : public views::View,
      public PickerTraversableItemContainer {
  METADATA_HEADER(PickerListItemContainerView, views::View)

 public:
  PickerListItemContainerView();
  PickerListItemContainerView(const PickerListItemContainerView&) = delete;
  PickerListItemContainerView& operator=(const PickerListItemContainerView&) =
      delete;
  ~PickerListItemContainerView() override;

  // PickerTraversableItemContainer:
  views::View* GetTopItem() override;
  views::View* GetBottomItem() override;
  views::View* GetItemAbove(views::View* item) override;
  views::View* GetItemBelow(views::View* item) override;
  views::View* GetItemLeftOf(views::View* item) override;
  views::View* GetItemRightOf(views::View* item) override;
  bool ContainsItem(views::View* item) override;

  PickerListItemView* AddListItem(
      std::unique_ptr<PickerListItemView> list_item);
  PickerItemWithSubmenuView* AddItemWithSubmenu(
      std::unique_ptr<PickerItemWithSubmenuView> item_with_submenu);

 private:
  views::ViewModelT<views::View> items_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_LIST_ITEM_CONTAINER_VIEW_H_
