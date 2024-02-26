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

class PickerItemView;
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
  explicit PickerSmallItemGridView(int grid_width);
  PickerSmallItemGridView(const PickerSmallItemGridView&) = delete;
  PickerSmallItemGridView& operator=(const PickerSmallItemGridView&) = delete;
  ~PickerSmallItemGridView() override;

  // PickerTraversableItemContainer:
  PickerItemView* GetTopItem() override;
  PickerItemView* GetBottomItem() override;
  PickerItemView* GetItemAbove(PickerItemView* item) override;
  PickerItemView* GetItemBelow(PickerItemView* item) override;
  PickerItemView* GetItemLeftOf(PickerItemView* item) override;
  PickerItemView* GetItemRightOf(PickerItemView* item) override;

  PickerEmojiItemView* AddEmojiItem(
      std::unique_ptr<PickerEmojiItemView> emoji_item);
  PickerSymbolItemView* AddSymbolItem(
      std::unique_ptr<PickerSymbolItemView> symbol_item);
  PickerEmoticonItemView* AddEmoticonItem(
      std::unique_ptr<PickerEmoticonItemView> emoticon_item);

 private:
  PickerItemView* AddSmallGridItem(
      std::unique_ptr<PickerItemView> small_grid_item);

  // Returns the row containing `item`, or nullptr if `item` is not part of this
  // grid.
  views::View* GetRowContaining(PickerItemView* item);

  int grid_width_ = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SMALL_ITEM_GRID_VIEW_H_
