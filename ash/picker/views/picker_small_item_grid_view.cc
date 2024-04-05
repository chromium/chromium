// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_small_item_grid_view.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_emoticon_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

// Horizontal padding between small grid items.
constexpr auto kSmallGridItemMargins = gfx::Insets::VH(0, 12);

// Padding around each row of small items.
constexpr auto kSmallGridItemRowMargins = gfx::Insets::TLBR(0, 16, 8, 16);

// Preferred size of small grid items.
constexpr gfx::Size kSmallGridItemPreferredSize(32, 32);

std::unique_ptr<views::View> CreateSmallItemsGridRow() {
  auto row = views::Builder<views::FlexLayoutView>()
                 .SetOrientation(views::LayoutOrientation::kHorizontal)
                 .SetMainAxisAlignment(views::LayoutAlignment::kStart)
                 .SetCollapseMargins(true)
                 .SetIgnoreDefaultMainAxisMargins(true)
                 .SetProperty(views::kMarginsKey, kSmallGridItemRowMargins)
                 .Build();
  row->SetDefault(views::kMarginsKey, kSmallGridItemMargins);
  return row;
}

}  // namespace

PickerSmallItemGridView::PickerSmallItemGridView(int grid_width)
    : grid_width_(grid_width) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  AddChildView(CreateSmallItemsGridRow());
}

PickerSmallItemGridView::~PickerSmallItemGridView() = default;

PickerItemView* PickerSmallItemGridView::GetTopItem() {
  if (children().empty()) {
    return nullptr;
  }
  // Return the first item in the top row, if it exists.
  views::View* row = children().front();
  return row->children().empty() ? nullptr
                                 : views::AsViewClass<PickerItemView>(
                                       row->children().front().get());
}

PickerItemView* PickerSmallItemGridView::GetBottomItem() {
  if (children().empty()) {
    return nullptr;
  }
  // Return the first item in the bottom row, if it exists.
  views::View* row = children().back();
  return row->children().empty() ? nullptr
                                 : views::AsViewClass<PickerItemView>(
                                       row->children().front().get());
}

PickerItemView* PickerSmallItemGridView::GetItemAbove(PickerItemView* item) {
  views::View* row = GetRowContaining(item);
  if (!row || row == children().front()) {
    return nullptr;
  }
  // Return the first item in the row above, if it exists.
  views::View* row_above =
      std::prev(base::ranges::find(children(), row))->get();
  return row_above->children().empty()
             ? nullptr
             : views::AsViewClass<PickerItemView>(
                   row_above->children().front().get());
}

PickerItemView* PickerSmallItemGridView::GetItemBelow(PickerItemView* item) {
  views::View* row = GetRowContaining(item);
  if (!row || row == children().back()) {
    return nullptr;
  }
  // Return the first item in the row below, if it exists.
  views::View* row_below =
      std::next(base::ranges::find(children(), row))->get();
  return row_below->children().empty()
             ? nullptr
             : views::AsViewClass<PickerItemView>(
                   row_below->children().front().get());
}

PickerItemView* PickerSmallItemGridView::GetItemLeftOf(PickerItemView* item) {
  views::View* row = GetRowContaining(item);
  if (!row || item == row->children().front()) {
    return nullptr;
  }
  return views::AsViewClass<PickerItemView>(
      std::prev(base::ranges::find(row->children(), item))->get());
}

PickerItemView* PickerSmallItemGridView::GetItemRightOf(PickerItemView* item) {
  views::View* row = GetRowContaining(item);
  if (!row || item == row->children().back()) {
    return nullptr;
  }
  return views::AsViewClass<PickerItemView>(
      std::next(base::ranges::find(row->children(), item))->get());
}

PickerEmojiItemView* PickerSmallItemGridView::AddEmojiItem(
    std::unique_ptr<PickerEmojiItemView> emoji_item) {
  emoji_item->SetPreferredSize(kSmallGridItemPreferredSize);
  return views::AsViewClass<PickerEmojiItemView>(
      AddSmallGridItem(std::move(emoji_item)));
}

PickerSymbolItemView* PickerSmallItemGridView::AddSymbolItem(
    std::unique_ptr<PickerSymbolItemView> symbol_item) {
  symbol_item->SetPreferredSize(kSmallGridItemPreferredSize);
  return views::AsViewClass<PickerSymbolItemView>(
      AddSmallGridItem(std::move(symbol_item)));
}

PickerEmoticonItemView* PickerSmallItemGridView::AddEmoticonItem(
    std::unique_ptr<PickerEmoticonItemView> emoticon_item) {
  emoticon_item->SetPreferredSize(
      gfx::Size(std::max(emoticon_item->GetPreferredSize().width(),
                         kSmallGridItemPreferredSize.width()),
                kSmallGridItemPreferredSize.height()));
  return views::AsViewClass<PickerEmoticonItemView>(
      AddSmallGridItem(std::move(emoticon_item)));
}

PickerItemView* PickerSmallItemGridView::AddSmallGridItem(
    std::unique_ptr<PickerItemView> grid_item) {
  // Try to add the item to the last row. If it doesn't fit, create a new row
  // and add the item there.
  views::View* row = children().back();
  if (!row->children().empty() &&
      row->GetPreferredSize().width() + kSmallGridItemMargins.left() +
              grid_item->GetPreferredSize().width() >
          grid_width_) {
    row = AddChildView(CreateSmallItemsGridRow());
  }
  return row->AddChildView(std::move(grid_item));
}

views::View* PickerSmallItemGridView::GetRowContaining(PickerItemView* item) {
  views::View* row = item->parent();
  return row && row->parent() == this ? row : nullptr;
}

BEGIN_METADATA(PickerSmallItemGridView)
END_METADATA

}  // namespace ash
