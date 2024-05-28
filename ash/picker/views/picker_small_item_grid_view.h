// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SMALL_ITEM_GRID_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SMALL_ITEM_GRID_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PickerEmojiItemView;
class PickerSymbolItemView;
class PickerEmoticonItemView;

// Container view for the small items in a section, which can include emoji,
// symbol and emoticon items. These are displayed in a grid of rows.
class ASH_EXPORT PickerSmallItemGridView
    : public views::View,
      public PickerTraversableItemContainer {
  METADATA_HEADER(PickerSmallItemGridView, views::View)

 public:
  explicit PickerSmallItemGridView(int grid_width, size_t max_visible_rows = 2);
  PickerSmallItemGridView(const PickerSmallItemGridView&) = delete;
  PickerSmallItemGridView& operator=(const PickerSmallItemGridView&) = delete;
  ~PickerSmallItemGridView() override;

  // PickerTraversableItemContainer:
  views::View* GetTopItem() override;
  views::View* GetBottomItem() override;
  views::View* GetItemAbove(views::View* item) override;
  views::View* GetItemBelow(views::View* item) override;
  views::View* GetItemLeftOf(views::View* item) override;
  views::View* GetItemRightOf(views::View* item) override;

  PickerEmojiItemView* AddEmojiItem(
      std::unique_ptr<PickerEmojiItemView> emoji_item);
  PickerSymbolItemView* AddSymbolItem(
      std::unique_ptr<PickerSymbolItemView> symbol_item);
  PickerEmoticonItemView* AddEmoticonItem(
      std::unique_ptr<PickerEmoticonItemView> emoticon_item);

 private:
  views::View* AddSmallGridItem(std::unique_ptr<views::View> small_grid_item);

  // Gets and returns the visible row containing `item`. Returns nullptr if the
  // row containing `item` is not visible or if `item` is not part of this grid.
  views::View* GetVisibleRowContaining(views::View* item);

  int grid_width_ = 0;

  // The maximum number of rows to show in the grid. Items that don't fit into
  // these rows are added to grid rows that are hidden by default.
  size_t max_visible_rows_ = 2;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SMALL_ITEM_GRID_VIEW_H_
