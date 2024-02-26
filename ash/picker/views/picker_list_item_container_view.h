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

namespace ash {

class PickerItemView;
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
  PickerItemView* GetTopItem() override;
  PickerItemView* GetBottomItem() override;
  PickerItemView* GetItemAbove(PickerItemView* item) override;
  PickerItemView* GetItemBelow(PickerItemView* item) override;
  PickerItemView* GetItemLeftOf(PickerItemView* item) override;
  PickerItemView* GetItemRightOf(PickerItemView* item) override;

  PickerListItemView* AddListItem(
      std::unique_ptr<PickerListItemView> list_item);
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_LIST_ITEM_CONTAINER_VIEW_H_
