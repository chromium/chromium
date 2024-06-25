// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_IMAGE_ITEM_GRID_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_IMAGE_ITEM_GRID_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PickerImageItemView;

// Container view for the image items in a section. The image items are
// displayed in a grid with two columns.
class ASH_EXPORT PickerImageItemGridView
    : public views::View,
      public PickerTraversableItemContainer {
  METADATA_HEADER(PickerImageItemGridView, views::View)

 public:
  explicit PickerImageItemGridView(int grid_width);
  PickerImageItemGridView(const PickerImageItemGridView&) = delete;
  PickerImageItemGridView& operator=(const PickerImageItemGridView&) = delete;
  ~PickerImageItemGridView() override;

  // PickerTraversableItemContainer:
  views::View* GetTopItem() override;
  views::View* GetBottomItem() override;
  views::View* GetItemAbove(views::View* item) override;
  views::View* GetItemBelow(views::View* item) override;
  views::View* GetItemLeftOf(views::View* item) override;
  views::View* GetItemRightOf(views::View* item) override;
  bool ContainsItem(views::View* item) override;

  PickerImageItemView* AddImageItem(
      std::unique_ptr<PickerImageItemView> image_item);

 private:
  // Returns the column containing `item`, or nullptr if `item` is not part of
  // this grid.
  views::View* GetColumnContaining(views::View* item);

  int grid_width_ = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_IMAGE_ITEM_GRID_VIEW_H_
