// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_IMAGE_ITEM_GRID_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_IMAGE_ITEM_GRID_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/quick_insert/views/quick_insert_traversable_item_container.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view.h"

namespace ash {

class QuickInsertImageItemView;

// Container view for the image items in a section. The image items are
// displayed in a grid with two columns.
class ASH_EXPORT QuickInsertImageItemGridView
    : public views::View,
      public QuickInsertTraversableItemContainer {
  METADATA_HEADER(QuickInsertImageItemGridView, views::View)

 public:
  explicit QuickInsertImageItemGridView(int grid_width,
                                        bool has_top_margin = true);
  QuickInsertImageItemGridView(const QuickInsertImageItemGridView&) = delete;
  QuickInsertImageItemGridView& operator=(const QuickInsertImageItemGridView&) =
      delete;
  ~QuickInsertImageItemGridView() override;

  // QuickInsertTraversableItemContainer:
  views::View* GetTopItem() override;
  views::View* GetBottomItem() override;
  views::View* GetItemAbove(views::View* item) override;
  views::View* GetItemBelow(views::View* item) override;
  views::View* GetItemLeftOf(views::View* item) override;
  views::View* GetItemRightOf(views::View* item) override;
  bool ContainsItem(views::View* item) override;

  QuickInsertImageItemView* AddImageItem(
      std::unique_ptr<QuickInsertImageItemView> image_item);

 private:
  // Returns the column containing `item`, or nullptr if `item` is not part of
  // this grid.
  views::View* GetColumnContaining(views::View* item);

  int column_width_;
  size_t num_items_ = 0;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_IMAGE_ITEM_GRID_VIEW_H_
